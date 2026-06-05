import { test, expect, request as apiRequest } from '@playwright/test';
import { WasmAppPage, APP_URL, BASE_URL } from './pages/WasmAppPage';

// ── HTTP asset checks ──────────────────────────────────────────────────────
test.describe('WASM assets are deployed', () => {
  test('qtloader.js returns 200', async ({ request }) => {
    const res = await request.get(`${BASE_URL}/qtloader.js`);
    expect(res.status(), 'qtloader.js should be deployed (200), not missing (404)').toBe(200);
  });

  test('MaximumTrainer.js returns 200', async ({ request }) => {
    const res = await request.get(`${BASE_URL}/MaximumTrainer.js`);
    expect(res.status(), 'MaximumTrainer.js should be deployed (200), not missing (404)').toBe(200);
  });

  test('MaximumTrainer.wasm returns 200', async ({ request }) => {
    const res = await request.get(`${BASE_URL}/MaximumTrainer.wasm`);
    expect(res.status(), 'MaximumTrainer.wasm should be deployed (200), not missing (404)').toBe(200);
  });

  test('logger.js returns 200', async ({ request }) => {
    const res = await request.get(`${BASE_URL}/logger.js`);
    expect(res.status(), 'logger.js should be deployed (200), not missing (404)').toBe(200);
  });
});

// ── Page-level checks ──────────────────────────────────────────────────────
test.describe('WASM webapp page', () => {
  test('"not deployed" message is absent', async ({ page }, testInfo) => {
    const wasmApp = new WasmAppPage(page);

    // Inject a full stub navigator.bluetooth so the page proceeds past the
    // browser-capability guard and attempts to load the WASM assets.
    await wasmApp.stubBluetooth();

    const errors: string[] = [];
    page.on('pageerror', (err) => errors.push(err.message));

    // Collect network failures for WASM asset URLs
    const failedRequests: string[] = [];
    page.on('requestfailed', (req) => {
      if (req.url().includes('qtloader') || req.url().includes('MaximumTrainer')) {
        failedRequests.push(req.url());
      }
    });

    await wasmApp.goto();

    // Give the JS time to attempt loading qtloader.js and react to success/failure
    await page.waitForTimeout(4000);

    // The "not deployed" sentinel text must NOT appear
    const notDeployedText = await page.locator('text=WebAssembly build hasn\'t been deployed yet').count();
    expect(notDeployedText, 'The "not deployed" fallback message should not be visible').toBe(0);

    // No WASM asset fetches should have failed
    expect(failedRequests, `These WASM assets failed to load: ${failedRequests.join(', ')}`).toHaveLength(0);

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/not-deployed-message-absent${retrySuffix}.png` });
  });

  test('loading screen or Qt canvas is present', async ({ page }, testInfo) => {
    const wasmApp = new WasmAppPage(page);
    await wasmApp.stubBluetooth();
    await wasmApp.goto();
    await page.waitForTimeout(3000);

    // Either the loading screen (progress bar) or the Qt canvas wrapper must be visible
    const loadingVisible = await wasmApp.loadingScreen.isVisible();
    const canvasVisible  = await page
      .locator('#qt-canvas-wrapper')
      .evaluate((el: HTMLElement) => el.style.visibility !== 'hidden');

    expect(loadingVisible || canvasVisible,
      'Either the loading screen or the Qt canvas should be visible').toBe(true);

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/loading-screen-or-canvas-present${retrySuffix}.png` });
  });

  test('WASM log overlay is present and has the copy button', async ({ page }, testInfo) => {
    const wasmApp = new WasmAppPage(page);
    await wasmApp.stubBluetooth();
    await wasmApp.goto();

    // Wait for the overlay AND the copy button to appear in the DOM.
    await expect(
      wasmApp.logOverlay.root,
      '#wasm-log-overlay should be present in the DOM',
    ).toHaveCount(1, { timeout: 15_000 });

    await expect(
      wasmApp.logOverlay.copyButton,
      '#wasm-log-copy-btn should be present inside the overlay',
    ).toHaveCount(1, { timeout: 5_000 });

    // Wait for at least one log line to be written, then verify.
    await expect(
      wasmApp.logOverlay.logLines().first(),
      'At least one log entry should appear',
    ).toBeVisible({ timeout: 10_000 });

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/wasm-log-overlay-present${retrySuffix}.png` });
  });
});

// ── BLE GATT ready callback ────────────────────────────────────────────────
test.describe('BLE GATT ready callback', () => {
  test.describe.configure({ timeout: 420_000 });
  test('page loads without errors when a mock async-GATT BLE device is present', async ({ page }, testInfo) => {
    const wasmApp = new WasmAppPage(page);

    // Inject a realistic mock BLE device that simulates the full async GATT
    // flow: requestDevice → gatt.connect() (with artificial latency) →
    // getPrimaryService → getCharacteristic → startNotifications.
    await page.addInitScript(() => {
      const delay = (ms: number) => new Promise<void>((r) => setTimeout(r, ms));

      const mockCharacteristic = {
        startNotifications: async () => {},
        addEventListener:   () => {},
      };
      const mockService = {
        getCharacteristic: async () => mockCharacteristic,
      };
      const mockGattServer: any = {
        connected: false,
        connect: async () => {
          await delay(150); // simulate async GATT connection latency
          mockGattServer.connected = true;
          return mockGattServer;
        },
        getPrimaryService: async () => mockService,
        disconnect: () => { mockGattServer.connected = false; },
      };
      const mockDevice = {
        name:             'MockTrainer',
        gatt:             mockGattServer,
        addEventListener: () => {},
      };

      Object.defineProperty(navigator, 'bluetooth', {
        value: {
          requestDevice:   async () => mockDevice,
          getAvailability: async () => true,
        },
        configurable: true,
      });
    });

    // Mock maximumtrainer.com backend APIs before navigation.
    const apiRequestedUrls = await wasmApp.mockBackendApis();

    // Install OAuth popup mock so the login dialog completes automatically.
    await wasmApp.setupOAuthMock();

    const consoleMessages: Array<{ type: string; text: string }> = [];
    page.on('console', (msg) =>
      consoleMessages.push({ type: msg.type(), text: msg.text() }),
    );

    await wasmApp.goto();

    // Wait until either the loading screen fades out or the Qt canvas becomes visible.
    await page.waitForFunction(
      () => {
        const loading = document.querySelector('#loading-screen');
        const canvas  = document.querySelector('#qt-canvas-wrapper');
        const loadingHidden  = loading && loading.classList.contains('hidden');
        const canvasVisible  = canvas && (canvas as HTMLElement).style.visibility !== 'hidden';
        return !!(loadingHidden || canvasVisible);
      },
      { timeout: 30_000 },
    );

    await page.waitForTimeout(3000);

    // Complete the OAuth login so the main window (and radio API) loads.
    await wasmApp.completeOAuthLogin();

    // No [MT] BLE error messages should have been emitted during loading
    const bleErrors = consoleMessages.filter(
      (m) => m.type === 'error' && m.text.includes('[MT]'),
    );
    expect(
      bleErrors,
      `Unexpected [MT] BLE errors with mock device: ${bleErrors.map((m) => m.text).join(', ')}`,
    ).toHaveLength(0);

    // No unexpected ERROR lines in the WASM log overlay
    const fatalErrors = await wasmApp.logOverlay.getFatalErrorLines();
    expect(
      fatalErrors,
      `WASM log overlay contains unexpected ERROR lines:\n${fatalErrors.join('\n')}`,
    ).toHaveLength(0);

    // No network-error WARN lines
    const notFoundLines = await wasmApp.logOverlay.getLinesContaining('Not Found');
    expect(
      notFoundLines,
      `WASM overlay shows "Not Found" — backend API requests are failing:\n${notFoundLines.join('\n')}`,
    ).toHaveLength(0);

    const statusCode0Lines = await wasmApp.logOverlay.getLinesContaining('HTTP status code 0');
    expect(
      statusCode0Lines,
      `WASM overlay shows "HTTP status code 0" — backend API requests are CORS-blocked:\n${statusCode0Lines.join('\n')}`,
    ).toHaveLength(0);

    const radioFetchFailedLines = await wasmApp.logOverlay.getLinesContaining('Radio list fetch failed');
    expect(
      radioFetchFailedLines,
      `WASM overlay shows "Radio list fetch failed" — radio API request failed:\n${radioFetchFailedLines.join('\n')}`,
    ).toHaveLength(0);

    // Verify a Qt backend API call was actually made
    expect(
      apiRequestedUrls.some((u) => u.includes('achievement_rest')),
      `Qt app did not call the achievement API — app may not have reached init phase.\nSeen URLs: ${apiRequestedUrls.join(', ')}`,
    ).toBe(true);

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/ble-gatt-ready-no-errors${retrySuffix}.png` });
  });
});

// ── Browser compatibility guard ────────────────────────────────────────────
test.describe('Browser compatibility guard', () => {
  test('compatibility warning is shown when getAvailability returns false', async ({ page }, testInfo) => {
    const wasmApp = new WasmAppPage(page);
    await wasmApp.stubBluetoothUnavailable();
    await wasmApp.goto();
    await page.waitForTimeout(2000);

    // Compatibility warning must be visible
    await expect(wasmApp.browserCompat.root).toBeVisible();

    // Loading screen must have faded out (opacity:0 via .hidden class).
    // toBeVisible() only checks display/visibility, not opacity, so we
    // verify the CSS class directly.
    await expect(wasmApp.loadingScreen.root).toHaveClass(/hidden/);

    // The detail paragraph must contain the hardware-unavailable message
    await expect(wasmApp.browserCompat.detail).toContainText('No Bluetooth adapter was detected');

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/compat-warning-no-adapter${retrySuffix}.png` });
  });

  test('compatibility warning is shown when navigator.bluetooth is absent', async ({ page }, testInfo) => {
    const wasmApp = new WasmAppPage(page);
    await wasmApp.stubBluetoothAbsent();
    await wasmApp.goto();
    await page.waitForTimeout(2000);

    // Compatibility warning must be visible
    await expect(wasmApp.browserCompat.root).toBeVisible();

    // The detail paragraph must contain the api-missing message
    await expect(wasmApp.browserCompat.detail).toContainText('Web Bluetooth API is not available');

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/compat-warning-no-bluetooth-api${retrySuffix}.png` });
  });
});

// ── PWA / manifest checks ──────────────────────────────────────────────────
test.describe('PWA support', () => {
  test('manifest.json returns 200', async ({ request }) => {
    const res = await request.get(`${BASE_URL}/manifest.json`);
    expect(res.status(), 'manifest.json should be deployed (200)').toBe(200);
  });

  test('manifest.json has required PWA fields', async ({ request }) => {
    const res = await request.get(`${BASE_URL}/manifest.json`);
    expect(res.status()).toBe(200);
    const manifest = await res.json() as Record<string, unknown>;
    expect(manifest['name'],       'manifest must have a name').toBeTruthy();
    expect(manifest['short_name'], 'manifest must have a short_name').toBeTruthy();
    expect(manifest['start_url'],  'manifest must have a start_url').toBeTruthy();
    expect(manifest['display'],    'manifest must have a display mode').toBeTruthy();
    expect(manifest['icons'],      'manifest must have icons array').toBeDefined();
    expect(Array.isArray(manifest['icons'])).toBe(true);
    expect((manifest['icons'] as unknown[]).length, 'manifest must have at least one icon').toBeGreaterThan(0);
  });

  test('service-worker.js returns 200', async ({ request }) => {
    const res = await request.get(`${BASE_URL}/service-worker.js`);
    expect(res.status(), 'service-worker.js should be deployed (200)').toBe(200);
  });

  test('app/index.html links to manifest', async ({ request }) => {
    const res = await request.get(APP_URL);
    expect(res.status()).toBe(200);
    const html = await res.text();
    expect(html, 'index.html should contain manifest link').toContain('manifest.json');
  });

  test('app/index.html has theme-color meta tag', async ({ request }) => {
    const res = await request.get(APP_URL);
    const html = await res.text();
    expect(html, 'index.html should have theme-color meta tag').toContain('theme-color');
  });
});

// ── BLE reconnect overlay checks ───────────────────────────────────────────
test.describe('BLE reconnect overlay', () => {
  test('reconnect overlay is present in the DOM and hidden by default', async ({ page }, testInfo) => {
    const wasmApp = new WasmAppPage(page);
    await wasmApp.stubBluetooth();
    await wasmApp.goto();

    // The overlay element must exist in the DOM
    await expect(wasmApp.bleReconnect.root).toHaveCount(1);

    // It must be hidden by default (display:none)
    const isVisible = await wasmApp.bleReconnect.isVisible();
    expect(isVisible, '#ble-reconnect-overlay should be hidden by default').toBe(false);

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/reconnect-overlay-hidden-by-default${retrySuffix}.png` });
  });

  test('reconnect overlay contains a Reconnect button', async ({ page }, testInfo) => {
    const wasmApp = new WasmAppPage(page);
    await wasmApp.stubBluetooth();
    await wasmApp.goto();

    await expect(wasmApp.bleReconnect.reconnectButton).toHaveCount(1);
    await expect(wasmApp.bleReconnect.reconnectButton).toHaveText(/Reconnect/i);

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/reconnect-overlay-has-reconnect-btn${retrySuffix}.png` });
  });

  test('reconnect overlay contains a dismiss button', async ({ page }, testInfo) => {
    const wasmApp = new WasmAppPage(page);
    await wasmApp.stubBluetooth();
    await wasmApp.goto();

    await expect(wasmApp.bleReconnect.dismissButton).toHaveCount(1);

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/reconnect-overlay-has-dismiss-btn${retrySuffix}.png` });
  });

  test('overlay becomes visible when shown programmatically and dismiss hides it', async ({ page }, testInfo) => {
    const wasmApp = new WasmAppPage(page);
    await wasmApp.stubBluetooth();
    await wasmApp.goto();

    // Show the overlay programmatically (simulates the gattserverdisconnected handler)
    await wasmApp.bleReconnect.show();
    await expect(wasmApp.bleReconnect.root).toBeVisible({ timeout: 2000 });

    // Reconnect and dismiss buttons must be visible inside the overlay
    await expect(wasmApp.bleReconnect.reconnectButton).toBeVisible();
    await expect(wasmApp.bleReconnect.dismissButton).toBeVisible();

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/reconnect-overlay-shown${retrySuffix}.png` });

    // Clicking dismiss must hide the overlay
    await wasmApp.bleReconnect.clickDismiss();
    await expect(wasmApp.bleReconnect.root).not.toBeVisible();

    await page.screenshot({ path: `test-results/wasm-screenshots/reconnect-overlay-dismissed${retrySuffix}.png` });
  });
});
