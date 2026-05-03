// @ts-check
const { test, expect } = require('@playwright/test');
const { getOverlayFatalErrorLines } = require('./wasm-test-helpers');

const BASE_ORIGIN = process.env.PLAYWRIGHT_BASE_URL || 'https://maximumtrainer.github.io/MaximumTrainer_Redux';
const APP_URL = `${BASE_ORIGIN}/app/`;

// ─── Recording Web Bluetooth mock ─────────────────────────────────────────────
//
// Replaces navigator.bluetooth with a recording mock that resolves all GATT
// calls successfully and captures which service / characteristic UUIDs were
// requested.  Assertions verify that the WASM bridge (webbluetooth_bridge.cpp:
// js_scanAndConnect + js_requestFtmsControl) invokes the Web Bluetooth API with
// the correct UUIDs for each sensor profile.
//
// Profile map (from webbluetooth_bridge.cpp):
//   0x180D → [0x2A37]   Heart Rate Measurement
//   0x1818 → [0x2A63]   Cycling Power Measurement
//   0x1816 → [0x2A5B]   CSC Measurement
//   0x1826 → [0x2AD2]   Indoor Bike Data  (+0x2AD9 FTMS Control Point write)
//   0xAAB0 → [0xAAB2]   Moxy Muscle Oxygen Measurement
async function injectRecordingBluetoothMock(page) {
  await page.addInitScript(() => {
    // Forward all index.html wasm-logger messages to the browser console so
    // Playwright's console event captures them.  Without this shim every
    // log() call in index.html is silently dropped (window._wasmLogger is
    // undefined) and the CI diagnostic dump shows nothing.
    window._wasmLogger = { log: (msg) => console.log('[wasm-load] ' + msg) };

    const calls = window._btleApiCalls = {
      requestDeviceFilters: undefined,
      getPrimaryService: [],
      getCharacteristic: [],
      startNotifications: [],
      writeValueWithResponse: []
    };

    // Factory: characteristic mock that records its UUID on each API call.
    const makeChar = (charUuid) => ({
      startNotifications: async () => {
        calls.startNotifications.push(charUuid);
      },
      addEventListener: () => {},
      writeValueWithResponse: async (bytes) => {
        calls.writeValueWithResponse.push({
          uuid: charUuid,
          bytes: Array.from(new Uint8Array(bytes))
        });
      }
    });

    // Factory: service mock whose getCharacteristic records each charUuid.
    const makeService = (_svcUuid) => ({
      getCharacteristic: async (charUuid) => {
        calls.getCharacteristic.push(charUuid);
        return makeChar(charUuid);
      }
    });

    const mockGattServer = {
      connected: true,
      connect: async () => {
        mockGattServer.connected = true;
        return mockGattServer;
      },
      getPrimaryService: async (svcUuid) => {
        calls.getPrimaryService.push(svcUuid);
        return makeService(svcUuid);
      },
      disconnect: () => { mockGattServer.connected = false; }
    };

    const mockDevice = {
      name: 'MockBleTrainer',
      gatt: mockGattServer,
      addEventListener: () => {}
    };

    Object.defineProperty(navigator, 'bluetooth', {
      value: {
        requestDevice: async (options) => {
          calls.requestDeviceFilters = (options && options.filters) ? options.filters : null;
          return mockDevice;
        },
        getAvailability: async () => true
      },
      configurable: true
    });
  });
}

// Screenshot output directory — screenshots are saved here so they can be
// uploaded as a CI artifact regardless of test pass/fail status.
const SCREENSHOT_DIR = 'test-results/wasm-screenshots';

// Wait for the WASM app to be ready for BLE testing.
//
// Two signals are checked — both are set inside index.html's onLoaded callback,
// which is triggered by qtloader.js at onRuntimeInitialized time:
//   1. Loading screen gains the 'hidden' class (set inside onLoaded)
//   2. Canvas wrapper's computed visibility becomes 'visible' (set inside onLoaded)
//
// NOTE: A previous version also checked window.mt_startBleScan (set during C++
// static init, which fires BEFORE onRuntimeInitialized).  That signal fires too
// early — the loading screen is still visible at that point, so screenshots
// captured at that moment always show the loading UI rather than the live canvas.
// By the time Signal 1 or 2 below fires, mt_startBleScan is already registered
// (static constructors ran first), so there is no loss of functionality.
//
// After onLoaded, a 3-second settling window lets Qt's main() start up and
// surface any initialisation errors (e.g. NaN from headless DPI detection)
// before we take screenshots or assert on the overlay.
//
// The WASM binary (~18 MB, ASYNCIFY-transformed) is compiled with --no-wasm-
// tier-up (Liftoff only) which cuts compile time from 3+ min to ~30 s on cold
// CI runners.  The 5-minute timeout is a generous safety margin.
//
// NOTE: pass `null` as the explicit arg so that the options object is correctly
// parsed as the third positional parameter by Playwright, not as the page
// function's argument.
async function waitForAppReady(page) {
  await page.waitForFunction(
    () => {
      // Signal 1: loading screen hidden (set inside onLoaded in index.html)
      const loadingScreen = document.querySelector('#loading-screen');
      if (loadingScreen && loadingScreen.classList.contains('hidden')) return true;
      // Signal 2: canvas wrapper visible by computed style (set inside onLoaded)
      const wrapper = document.querySelector('#qt-canvas-wrapper');
      return !!(wrapper && window.getComputedStyle(wrapper).visibility === 'visible');
    },
    null,          // no argument passed to the page function
    { timeout: 300_000 }   // 5 min — generous safety margin for cold CI runners
  );
  // Allow Qt's main() a brief window to start up and surface any initialisation
  // errors before we capture screenshots or run assertions on the overlay.
  await page.waitForTimeout(3000);
}

// Trigger BLE scanning via the test helper registered by BtleHubWasm::ctor
// and wait for requestDevice to be called — the earliest reliable signal that
// the WASM bridge has initiated scanning.
async function triggerBleScanAndWaitForRequestDevice(page) {
  // Fail fast if the deployed app didn't expose the expected test hook.
  const hasHook = await page.evaluate(() => typeof window.mt_startBleScan === 'function');
  expect(hasHook, 'window.mt_startBleScan was not registered by the app build').toBeTruthy();

  await page.evaluate(() => window.mt_startBleScan());

  // Earliest reliable signal: our injected mock recorded a requestDevice call.
  // requestDeviceFilters starts as `undefined`; it becomes defined (even if null
  // for a filter-less call) the moment requestDevice is invoked.
  await page.waitForFunction(
    () => {
      const calls = window._btleApiCalls;
      return calls && calls.requestDeviceFilters !== undefined;
    },
    null,
    { timeout: 45_000 }
  );
}

// Wait until the full async GATT setup chain has completed.
// The last step recorded by the mock is writeValueWithResponse on the
// FTMS Control Point (0x2AD9), which only fires after subscribeAll and
// requestFtmsControl both finish.  Polling for it is more reliable than
// a fixed delay.
async function waitForFtmsWrite(page) {
  await page.waitForFunction(
    () => {
      const calls = window._btleApiCalls;
      return calls &&
        Array.isArray(calls.writeValueWithResponse) &&
        calls.writeValueWithResponse.length > 0;
    },
    null,
    { timeout: 45_000 }
  );
}

// ─── Tests ────────────────────────────────────────────────────────────────────

test.describe('WASM BLE API — Web Bluetooth call verification', () => {

  // The WASM binary (~18 MB, ASYNCIFY-transformed) can take 2+ minutes to
  // JIT-compile on cold CI runners.  Loading it once in beforeAll and sharing
  // the page across all four tests avoids repeating that cost four times.
  //
  // describe timeout: 7 min — applies to beforeAll hooks AND individual tests.
  // test.beforeAll(fn, { timeout }) is silently ignored in some Playwright
  // versions when no describe-level timeout is set; test.describe.configure is
  // the authoritative override that Playwright docs guarantee for hooks.
  // Individual test assertions complete in <60 s; the 7 min ceiling causes no harm.
  test.describe.configure({ timeout: 420_000 });

  /** @type {import('@playwright/test').Page} */
  let sharedPage;
  /** @type {import('@playwright/test').BrowserContext} */
  let sharedContext;
  const consoleLogs = [];
  const pageErrors = [];
  const failedRequests = [];

  test.beforeAll(async ({ browser }) => {
    // Extend timeout for this hook — WASM JIT compilation takes 3+ min on cold
    // CI runners.  test.setTimeout() inside a beforeAll changes the currently-
    // running hook's deadline; this is the only approach that works reliably
    // across all Playwright versions (test.describe.configure and the
    // beforeAll { timeout } option are silently ignored in some versions).
    test.setTimeout(420_000);
    sharedContext = await browser.newContext();
    sharedPage = await sharedContext.newPage();
    sharedPage.on('console', msg => consoleLogs.push({ type: msg.type(), text: msg.text() }));
    sharedPage.on('pageerror', err => pageErrors.push(err.message));
    sharedPage.on('requestfailed', req => failedRequests.push(`${req.url()} — ${req.failure()?.errorText}`));
    await injectRecordingBluetoothMock(sharedPage);
    await sharedPage.goto(APP_URL, { waitUntil: 'domcontentloaded' });
    try {
      await waitForAppReady(sharedPage);
    } catch (loadErr) {
      // Dump all available diagnostics so CI logs show exactly what happened.
      console.error('[diagnostic] waitForAppReady timed out.');
      console.error('[diagnostic] Page console output:');
      for (const entry of consoleLogs) {
        console.error(`  [${entry.type}] ${entry.text}`);
      }
      if (pageErrors.length) {
        console.error('[diagnostic] JavaScript errors:');
        for (const e of pageErrors) console.error(`  [pageerror] ${e}`);
      }
      if (failedRequests.length) {
        console.error('[diagnostic] Failed network requests:');
        for (const r of failedRequests) console.error(`  [req-fail] ${r}`);
      }
      const ev = async (fn, label) => sharedPage.evaluate(fn).catch(e => `<eval failed: ${e.message}>`);
      const isSecure     = await ev(() => window.isSecureContext,                     'isSecureContext');
      const hasBt        = await ev(() => typeof navigator.bluetooth,                 'navigator.bluetooth');
      const hasScan      = await ev(() => typeof window.mt_startBleScan,              'mt_startBleScan');
      const hasQtLoader  = await ev(() => typeof window.QtLoader,                     'QtLoader');
      const hasAppInst   = await ev(() => typeof window.createQtAppInstance,          'createQtAppInstance');
      const loadingText  = await ev(() => document.getElementById('loading-status')?.textContent, 'loading-status');
      const loadingClass = await ev(() => document.getElementById('loading-screen')?.className,   'loading-screen class');
      console.error(
        `[diagnostic] isSecureContext=${isSecure} navigator.bluetooth=${hasBt} ` +
        `mt_startBleScan=${hasScan} QtLoader=${hasQtLoader} ` +
        `createQtAppInstance=${hasAppInst}`
      );
      console.error(`[diagnostic] loading-screen class="${loadingClass}", status text="${loadingText}"`);
      throw loadErr;
    }

    // Capture settled app state — canvas is visible, main() has had 3 s to run.
    // This screenshot is always saved (regardless of test pass/fail) so CI has
    // proof of what the app looked like after initialisation completed.
    await sharedPage.screenshot({ path: `${SCREENSHOT_DIR}/wasm-app-ready.png` });

    // Assert that Qt's initialisation did not emit a 'number NaN' startup error.
    // logger.js captures window.onerror and console.error into the overlay.
    // index.html's qtLoad catch handler logs 'Failed to start: TypeError: …'.
    // Both sources are visible in #wasm-log-overlay after the 3-s settling window.
    const overlayText = await sharedPage.evaluate(() => {
      const el = document.querySelector('#wasm-log-overlay');
      return el ? el.textContent : '';
    });
    expect(overlayText,
      `WASM initialisation produced a NaN error — check Qt DPI / canvas setup:\n${overlayText}`)
      .not.toContain('number NaN');
    expect(overlayText,
      `WASM initialisation failed to start — check Qt entryFunction / ASYNCIFY:\n${overlayText}`)
      .not.toContain('Failed to start');

    // Assert that no unexpected ERROR lines appear in the overlay.
    // logger.js classifies Qt qWarning() messages as WARN (not ERROR) since they
    // contain '] [WARN] ['. This assertion catches genuine fatal errors (JS
    // exceptions, WASM instantiation failures) while ignoring Qt runtime warnings.
    // Known browser-level noise (backend unreachable, User-Agent restriction) is
    // excluded by getOverlayFatalErrorLines.
    const fatalErrors = await getOverlayFatalErrorLines(sharedPage);
    expect(fatalErrors,
      `WASM log overlay contains unexpected ERROR lines:\n${fatalErrors.join('\n')}`)
      .toHaveLength(0);
  }, { timeout: 420_000 }); // 7 min: WASM JIT-compilation takes 3+ min on cold CI runners

  test.afterAll(async () => {
    await sharedPage?.close();
    await sharedContext?.close();
  });

  // Reset the recording mock before each BLE test so every assertion starts
  // with a clean slate (all call arrays empty, requestDeviceFilters undefined).
  //
  // IMPORTANT: mutate the existing _btleApiCalls object in-place rather than
  // replacing it.  The mock's requestDevice closure captured the original
  // `calls` reference at init-script time; if we assign a new object to
  // window._btleApiCalls the mock would keep writing to the old one while
  // waitForFunction reads from the new one, and every check would time out.
  async function resetRecordingMock() {
    consoleLogs.length = 0;
    pageErrors.length = 0;
    failedRequests.length = 0;
    await sharedPage.evaluate(() => {
      const c = window._btleApiCalls;
      c.requestDeviceFilters  = undefined;
      c.getPrimaryService     = [];
      c.getCharacteristic     = [];
      c.startNotifications    = [];
      c.writeValueWithResponse = [];
    });
  }

  // ── requestDevice service filters ──────────────────────────────────────────
  test('requestDevice is called with correct service filter UUIDs', async ({}, testInfo) => {
    await resetRecordingMock();
    await triggerBleScanAndWaitForRequestDevice(sharedPage);

    const recorded = await sharedPage.evaluate(() => window._btleApiCalls);

    // requestDevice must have been called by js_scanAndConnect
    expect(recorded.requestDeviceFilters,
      'navigator.bluetooth.requestDevice() was not called — BLE scan was not initiated')
      .not.toBeNull();

    // Service filter UUIDs must include the four core BLE sensor profiles
    const filterServices = (recorded.requestDeviceFilters || [])
      .flatMap(f => f.services || []);

    for (const svcUuid of [0x180D, 0x1816, 0x1818, 0x1826]) {
      expect(filterServices,
        `requestDevice filter must include service 0x${svcUuid.toString(16).toUpperCase()}`)
        .toContain(svcUuid);
    }

    // No [MT] BLE error messages should appear during a successful mock connection
    const bleErrors = consoleLogs.filter(m => m.type === 'error' && m.text.includes('[MT]'));
    expect(bleErrors,
      `Unexpected [MT] BLE errors: ${bleErrors.map(m => m.text).join(', ')}`)
      .toHaveLength(0);

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await sharedPage.screenshot({ path: `test-results/wasm-screenshots/ble-api-request-device-filters${retrySuffix}.png` });
  });

  // ── getPrimaryService for each sensor profile ──────────────────────────────
  test('getPrimaryService is called for each sensor profile', async ({}, testInfo) => {
    await resetRecordingMock();
    await triggerBleScanAndWaitForRequestDevice(sharedPage);

    const recorded = await sharedPage.evaluate(() => window._btleApiCalls);

    // subscribeAll iterates over all services in profileMap; js_requestFtmsControl
    // also calls getPrimaryService(0x1826) separately.
    for (const svcUuid of [0x180D, 0x1816, 0x1818, 0x1826]) {
      expect(recorded.getPrimaryService,
        `getPrimaryService(0x${svcUuid.toString(16).toUpperCase()}) was not called`)
        .toContain(svcUuid);
    }

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await sharedPage.screenshot({ path: `test-results/wasm-screenshots/ble-api-get-primary-service${retrySuffix}.png` });
  });

  // ── startNotifications for each sensor characteristic ─────────────────────
  test('startNotifications is called for each sensor characteristic', async ({}, testInfo) => {
    await resetRecordingMock();
    await triggerBleScanAndWaitForRequestDevice(sharedPage);

    const recorded = await sharedPage.evaluate(() => window._btleApiCalls);

    // One startNotifications per characteristic in profileMap
    for (const charUuid of [0x2A37, 0x2A5B, 0x2A63, 0x2AD2]) {
      expect(recorded.startNotifications,
        `startNotifications was not called for 0x${charUuid.toString(16).toUpperCase()}`)
        .toContain(charUuid);
    }

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await sharedPage.screenshot({ path: `test-results/wasm-screenshots/ble-api-start-notifications${retrySuffix}.png` });
  });

  // ── FTMS Request Control write ─────────────────────────────────────────────
  test('FTMS Request Control (opcode 0x00) is written to characteristic 0x2AD9', async ({}, testInfo) => {
    await resetRecordingMock();
    await triggerBleScanAndWaitForRequestDevice(sharedPage);
    // BtleHubWasm is only instantiated when a workout runs, so g_connectedCallback
    // is never set during Playwright CI.  Call the dedicated test hook
    // mt_requestFtmsControl() which invokes js_requestFtmsControl() directly,
    // bypassing the C++ callback chain but testing the same JS UUID/opcode logic.
    const hasFtmsHook = await sharedPage.evaluate(() => typeof window.mt_requestFtmsControl === 'function');
    expect(hasFtmsHook,
      'window.mt_requestFtmsControl hook absent — WASM build must be rebuilt with updated js_exposeTestScanApi')
      .toBeTruthy();
    await sharedPage.evaluate(() => window.mt_requestFtmsControl());
    await waitForFtmsWrite(sharedPage);

    const recorded = await sharedPage.evaluate(() => window._btleApiCalls);

    // js_requestFtmsControl writes [0x00] to FTMS Control Point (0x2AD9)
    const ftmsWrite = (recorded.writeValueWithResponse || [])
      .find(w => w.uuid === 0x2AD9 && w.bytes[0] === 0x00);

    expect(ftmsWrite,
      'writeValueWithResponse([0x00]) on FTMS Control Point 0x2AD9 was not called')
      .toBeDefined();

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await sharedPage.screenshot({ path: `test-results/wasm-screenshots/ble-api-ftms-request-control${retrySuffix}.png` });
  });
});
