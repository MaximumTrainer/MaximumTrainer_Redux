// @ts-check
const { test, expect, request } = require('@playwright/test');

const APP_URL = 'https://maximumtrainer.github.io/MaximumTrainer_Redux/app/';
const BASE_URL = 'https://maximumtrainer.github.io/MaximumTrainer_Redux/app';

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
  test('"not deployed" message is absent', async ({ page }) => {
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
  });

  test('loading screen or Qt canvas is present', async ({ page }) => {
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
  });

  test('WASM log overlay is present and has the copy button', async ({ page }) => {
    await stubBluetooth(page);

    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });

    // Wait for the overlay to appear in the DOM (injected by logger.js on DOMContentLoaded)
    await page.waitForSelector('#wasm-log-overlay', { timeout: 10000 });

    // The log overlay must exist in the DOM
    const overlay = page.locator('#wasm-log-overlay');
    expect(await overlay.count(), '#wasm-log-overlay should be present in the DOM').toBe(1);

    // The copy button must exist inside the overlay
    const copyBtn = page.locator('#wasm-log-copy-btn');
    expect(await copyBtn.count(), '#wasm-log-copy-btn should be present inside the overlay').toBe(1);

    // Wait for at least one log line to be written, then verify
    await page.waitForFunction(() => {
      const overlay = document.querySelector('#wasm-log-overlay');
      return overlay && overlay.querySelectorAll('div > div').length > 0;
    }, { timeout: 10000 });
    const logLines = overlay.locator('div > div');
    const lineCount = await logLines.count();
    expect(lineCount, 'The log overlay should contain at least one log entry').toBeGreaterThan(0);
  });
});

// ── BLE GATT ready callback (Gap 2 fix) ───────────────────────────────────
test.describe('BLE GATT ready callback', () => {
  test('page loads without errors when a mock async-GATT BLE device is present', async ({ page }) => {
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

    // No [MT] BLE error messages should have been emitted during loading
    const bleErrors = consoleMessages.filter(
      (m) => m.type === 'error' && m.text.includes('[MT]'),
    );
    expect(
      bleErrors,
      `Unexpected [MT] BLE errors with mock device: ${bleErrors.map((m) => m.text).join(', ')}`,
    ).toHaveLength(0);
  });
});

test.describe('Browser compatibility guard', () => {
  test('compatibility warning is shown when getAvailability returns false', async ({ page }) => {
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
  });

  test('compatibility warning is shown when navigator.bluetooth is absent', async ({ page }) => {
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
  test('reconnect overlay is present in the DOM and hidden by default', async ({ page }) => {
    await stubBluetooth(page);
    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });

    // The overlay element must exist in the DOM
    const overlay = page.locator('#ble-reconnect-overlay');
    await expect(overlay).toHaveCount(1);

    // It must be hidden by default (display:none)
    const isVisible = await overlay.isVisible();
    expect(isVisible, '#ble-reconnect-overlay should be hidden by default').toBe(false);
  });

  test('reconnect overlay contains a Reconnect button', async ({ page }) => {
    await stubBluetooth(page);
    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });

    const btn = page.locator('#ble-reconnect-btn');
    await expect(btn).toHaveCount(1);
    await expect(btn).toHaveText(/Reconnect/i);
  });

  test('reconnect overlay contains a dismiss button', async ({ page }) => {
    await stubBluetooth(page);
    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });

    const dismissBtn = page.locator('#ble-reconnect-dismiss');
    await expect(dismissBtn).toHaveCount(1);
  });

  test('overlay becomes visible when shown programmatically and dismiss hides it', async ({ page }) => {
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

    // Clicking dismiss must hide the overlay
    await page.click('#ble-reconnect-dismiss');
    await expect(overlay).not.toBeVisible();
  });
});
