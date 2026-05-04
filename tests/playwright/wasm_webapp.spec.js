// @ts-check
const { test, expect, request } = require('@playwright/test');
const { getOverlayFatalErrorLines, getOverlayLinesContaining, mockBackendApis } = require('./wasm-test-helpers');

const BASE_ORIGIN = process.env.PLAYWRIGHT_BASE_URL || 'https://maximumtrainer.github.io/MaximumTrainer_Redux';
const APP_URL = `${BASE_ORIGIN}/app/`;
const BASE_URL = `${BASE_ORIGIN}/app`;

// Helper: inject a full Web Bluetooth stub that passes the capability check
async function stubBluetooth(page) {
  await page.addInitScript(() => {
    if (!navigator.bluetooth) {
      Object.defineProperty(navigator, 'bluetooth', {
        value: {
          requestDevice: async () => { throw new Error('stub'); },
          getAvailability: async () => true,
        },
        configurable: true,
      });
    }
  });
}

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
    // Inject a full stub navigator.bluetooth so the page proceeds past the
    // browser-capability guard and attempts to load the WASM assets.
    await stubBluetooth(page);

    const errors = [];
    page.on('pageerror', err => errors.push(err.message));

    // Collect network failures for WASM asset URLs
    const failedRequests = [];
    page.on('requestfailed', req => {
      if (req.url().includes('qtloader') || req.url().includes('MaximumTrainer')) {
        failedRequests.push(req.url());
      }
    });

    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });

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
    await stubBluetooth(page);

    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
    await page.waitForTimeout(3000);

    // Either the loading screen (progress bar) or the Qt canvas wrapper must be visible
    const loadingScreen = page.locator('#loading-screen');
    const canvasWrapper = page.locator('#qt-canvas-wrapper');

    const loadingVisible = await loadingScreen.isVisible();
    const canvasVisible  = await canvasWrapper.evaluate(el => el.style.visibility !== 'hidden');

    expect(loadingVisible || canvasVisible,
      'Either the loading screen or the Qt canvas should be visible').toBe(true);

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/loading-screen-or-canvas-present${retrySuffix}.png` });
  });

  test('WASM log overlay is present and has the copy button', async ({ page }, testInfo) => {
    await stubBluetooth(page);

    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });

    // Wait for the overlay AND the copy button to appear in the DOM.
    // Both are added synchronously by logger.js, but we use Playwright's
    // auto-retrying assertions to avoid one-shot count checks that can flake
    // on slow CI runners.
    const overlay  = page.locator('#wasm-log-overlay');
    const copyBtn  = page.locator('#wasm-log-copy-btn');

    await expect(overlay,  '#wasm-log-overlay should be present in the DOM').toHaveCount(1, { timeout: 15_000 });
    await expect(copyBtn, '#wasm-log-copy-btn should be present inside the overlay').toHaveCount(1, { timeout: 5_000 });

    // Wait for at least one log line to be written, then verify.
    const logLines = overlay.locator('div > div');
    await expect(logLines.first(), 'At least one log entry should appear').toBeVisible({ timeout: 10_000 });

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/wasm-log-overlay-present${retrySuffix}.png` });
  });
});

// ── BLE GATT ready callback (Gap 2 fix) ───────────────────────────────────
test.describe('BLE GATT ready callback', () => {
  test('page loads without errors when a mock async-GATT BLE device is present', async ({ page }, testInfo) => {
    // Inject a realistic mock BLE device that simulates the full async GATT
    // flow: requestDevice → gatt.connect() (with artificial latency) →
    // getPrimaryService → getCharacteristic → startNotifications.
    // This validates that the bridge correctly handles async GATT setup and
    // does not emit deviceConnected before the connection is truly ready.
    await page.addInitScript(() => {
      const delay = (ms) => new Promise((r) => setTimeout(r, ms));

      const mockCharacteristic = {
        startNotifications: async () => {},
        addEventListener: () => {},
      };
      const mockService = {
        getCharacteristic: async () => mockCharacteristic,
      };
      const mockGattServer = {
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
        name: 'MockTrainer',
        gatt: mockGattServer,
        addEventListener: () => {},
      };

      Object.defineProperty(navigator, 'bluetooth', {
        value: {
          requestDevice: async () => mockDevice,
          getAvailability: async () => true,
        },
        configurable: true,
      });
    });

    // Mock maximumtrainer.com backend APIs before navigation so Qt's radio /
    // achievement requests return 200 and do not produce WARN overlay lines.
    const apiRequestedUrls = await mockBackendApis(page);

    const consoleMessages = [];
    page.on('console', (msg) => consoleMessages.push({ type: msg.type(), text: msg.text() }));

    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });

    // Wait until either the loading screen fades out (opacity → 0) or the
    // Qt canvas becomes visible — either signals that the BLE check passed.
    await page.waitForFunction(
      () => {
        const loading = document.querySelector('#loading-screen');
        const canvas = document.querySelector('#qt-canvas-wrapper');
        const loadingHidden = loading && loading.classList.contains('hidden');
        const canvasVisible = canvas && canvas.style.visibility !== 'hidden';
        return !!(loadingHidden || canvasVisible);
      },
      { timeout: 30000 },
    );

    // Give Qt's main() a brief window to complete network requests before asserting.
    await page.waitForTimeout(3000);

    // No [MT] BLE error messages should have been emitted during loading
    const bleErrors = consoleMessages.filter(
      (m) => m.type === 'error' && m.text.includes('[MT]'),
    );
    expect(
      bleErrors,
      `Unexpected [MT] BLE errors with mock device: ${bleErrors.map((m) => m.text).join(', ')}`,
    ).toHaveLength(0);

    // No unexpected ERROR lines in the WASM log overlay (Qt [WARN] messages are
    // classified as WARN by logger.js after the '] [WARN] [' fix).
    const fatalErrors = await getOverlayFatalErrorLines(page);
    expect(fatalErrors,
      `WASM log overlay contains unexpected ERROR lines:\n${fatalErrors.join('\n')}`)
      .toHaveLength(0);

    // Assert no network-error WARN lines — with mockBackendApis() registered all
    // maximumtrainer.com requests return 200 so these must not appear in the overlay.
    const notFoundLines = await getOverlayLinesContaining(page, 'Not Found');
    expect(notFoundLines,
      `WASM overlay shows "Not Found" — backend API requests are failing:\n${notFoundLines.join('\n')}`)
      .toHaveLength(0);

    const statusCode0Lines = await getOverlayLinesContaining(page, 'HTTP status code 0');
    expect(statusCode0Lines,
      `WASM overlay shows "HTTP status code 0" — backend API requests are CORS-blocked:\n${statusCode0Lines.join('\n')}`)
      .toHaveLength(0);

    const radioFetchFailedLines = await getOverlayLinesContaining(page, 'Radio list fetch failed');
    expect(radioFetchFailedLines,
      `WASM overlay shows "Radio list fetch failed" — radio API request failed:\n${radioFetchFailedLines.join('\n')}`)
      .toHaveLength(0);

    // Verify the Qt backend API calls were actually made (proves app reached the
    // MainWindow network-initialisation phase, equivalent to the native desktop).
    expect(apiRequestedUrls.some(u => u.includes('radio_rest')),
      `Qt MainWindow did not call the radio API — app may not have reached init phase.\nSeen URLs: ${apiRequestedUrls.join(', ')}`)
      .toBe(true);

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/ble-gatt-ready-no-errors${retrySuffix}.png` });
  });
});

test.describe('Browser compatibility guard', () => {
  test('compatibility warning is shown when getAvailability returns false', async ({ page }, testInfo) => {
    // Stub navigator.bluetooth with getAvailability returning false (hardware off)
    await page.addInitScript(() => {
      Object.defineProperty(navigator, 'bluetooth', {
        value: {
          requestDevice: async () => { throw new Error('stub'); },
          getAvailability: async () => false,
        },
        configurable: true,
      });
    });

    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
    await page.waitForTimeout(2000);

    // Compatibility warning must be visible
    const warning = page.locator('#browser-warning');
    await expect(warning).toBeVisible();

    // Loading screen must have faded out (opacity:0 via .hidden class).
    // Playwright's toBeVisible() only checks display/visibility, not opacity,
    // so we verify the CSS class directly.
    const loadingScreen = page.locator('#loading-screen');
    await expect(loadingScreen).toHaveClass(/hidden/);

    // The detail paragraph must contain the hardware-unavailable message
    const detail = page.locator('#browser-warning-detail');
    await expect(detail).toContainText('No Bluetooth adapter was detected');

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/compat-warning-no-adapter${retrySuffix}.png` });
  });

  test('compatibility warning is shown when navigator.bluetooth is absent', async ({ page }, testInfo) => {
    // Remove navigator.bluetooth to simulate an unsupported browser (e.g. Firefox)
    await page.addInitScript(() => {
      Object.defineProperty(navigator, 'bluetooth', {
        value: undefined,
        configurable: true,
      });
    });

    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
    await page.waitForTimeout(2000);

    // Compatibility warning must be visible
    const warning = page.locator('#browser-warning');
    await expect(warning).toBeVisible();

    // The detail paragraph must contain the api-missing message
    const detail = page.locator('#browser-warning-detail');
    await expect(detail).toContainText('Web Bluetooth API is not available');

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/compat-warning-no-bluetooth-api${retrySuffix}.png` });
  });
});

// ── PWA / manifest checks ─────────────────────────────────────────────────
test.describe('PWA support', () => {
  test('manifest.json returns 200', async ({ request }) => {
    const res = await request.get(`${BASE_URL}/manifest.json`);
    expect(res.status(), 'manifest.json should be deployed (200)').toBe(200);
  });

  test('manifest.json has required PWA fields', async ({ request }) => {
    const res = await request.get(`${BASE_URL}/manifest.json`);
    expect(res.status()).toBe(200);
    const manifest = await res.json();
    expect(manifest.name, 'manifest must have a name').toBeTruthy();
    expect(manifest.short_name, 'manifest must have a short_name').toBeTruthy();
    expect(manifest.start_url, 'manifest must have a start_url').toBeTruthy();
    expect(manifest.display, 'manifest must have a display mode').toBeTruthy();
    expect(manifest.icons, 'manifest must have icons array').toBeDefined();
    expect(Array.isArray(manifest.icons)).toBe(true);
    expect(manifest.icons.length, 'manifest must have at least one icon').toBeGreaterThan(0);
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

// ── BLE reconnect overlay checks ──────────────────────────────────────────
test.describe('BLE reconnect overlay', () => {
  test('reconnect overlay is present in the DOM and hidden by default', async ({ page }, testInfo) => {
    await stubBluetooth(page);
    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });

    // The overlay element must exist in the DOM
    const overlay = page.locator('#ble-reconnect-overlay');
    await expect(overlay).toHaveCount(1);

    // It must be hidden by default (display:none)
    const isVisible = await overlay.isVisible();
    expect(isVisible, '#ble-reconnect-overlay should be hidden by default').toBe(false);

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/reconnect-overlay-hidden-by-default${retrySuffix}.png` });
  });

  test('reconnect overlay contains a Reconnect button', async ({ page }, testInfo) => {
    await stubBluetooth(page);
    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });

    const btn = page.locator('#ble-reconnect-btn');
    await expect(btn).toHaveCount(1);
    await expect(btn).toHaveText(/Reconnect/i);

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/reconnect-overlay-has-reconnect-btn${retrySuffix}.png` });
  });

  test('reconnect overlay contains a dismiss button', async ({ page }, testInfo) => {
    await stubBluetooth(page);
    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });

    const dismissBtn = page.locator('#ble-reconnect-dismiss');
    await expect(dismissBtn).toHaveCount(1);

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/reconnect-overlay-has-dismiss-btn${retrySuffix}.png` });
  });

  test('overlay becomes visible when shown programmatically and dismiss hides it', async ({ page }, testInfo) => {
    await stubBluetooth(page);
    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });

    // Show the overlay by setting display to flex (simulates what the JS
    // gattserverdisconnected handler does when auto-reconnect is exhausted)
    await page.evaluate(() => {
      const overlay = document.getElementById('ble-reconnect-overlay');
      if (overlay) overlay.style.display = 'flex';
    });

    const overlay = page.locator('#ble-reconnect-overlay');
    await expect(overlay).toBeVisible({ timeout: 2000 });

    // Reconnect and dismiss buttons must be visible inside the overlay
    await expect(page.locator('#ble-reconnect-btn')).toBeVisible();
    await expect(page.locator('#ble-reconnect-dismiss')).toBeVisible();

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await page.screenshot({ path: `test-results/wasm-screenshots/reconnect-overlay-shown${retrySuffix}.png` });

    // Clicking dismiss must hide the overlay
    await page.click('#ble-reconnect-dismiss');
    await expect(overlay).not.toBeVisible();

    await page.screenshot({ path: `test-results/wasm-screenshots/reconnect-overlay-dismissed${retrySuffix}.png` });
  });
});
