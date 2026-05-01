// @ts-check
const { test, expect } = require('@playwright/test');

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

// Wait for the WASM app to be ready for BLE testing.
//
// Three independent signals are checked — any one being true means the app
// has reached a state where window.mt_startBleScan is callable:
//   1. window.mt_startBleScan is defined (set by WasmTestScanApiRegistrar
//      during C++ static init — fires before onRuntimeInitialized, so this
//      is the earliest possible reliable signal)
//   2. Loading screen gains the 'hidden' class (set inside onLoaded)
//   3. Canvas wrapper's computed visibility becomes 'visible' (set inside onLoaded)
//
// The WASM binary (~18 MB, ASYNCIFY-transformed) can take 3+ minutes to JIT-
// compile on cold CI runners (observed: ~184 s on GitHub Actions ubuntu-latest).
// The timeout is set generously here; the calling context (beforeAll) carries
// its own 7-minute outer limit set via test.setTimeout().
//
// NOTE: pass `null` as the explicit arg so that the options object is correctly
// parsed as the third positional parameter by Playwright, not as the page
// function's argument.
async function waitForAppReady(page) {
  await page.waitForFunction(
    () => {
      // Signal 1: test hook registered by C++ static initializer (before onLoaded)
      if (typeof window.mt_startBleScan === 'function') return true;
      // Signal 2: loading screen hidden (onLoaded in index.html)
      const loadingScreen = document.querySelector('#loading-screen');
      if (loadingScreen && loadingScreen.classList.contains('hidden')) return true;
      // Signal 3: canvas wrapper visible by computed style (onLoaded in index.html)
      const wrapper = document.querySelector('#qt-canvas-wrapper');
      return !!(wrapper && window.getComputedStyle(wrapper).visibility === 'visible');
    },
    null,          // no argument passed to the page function
    { timeout: 300_000 }   // 5 min — WASM JIT takes 3+ min on cold CI runners
  );
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
    await injectRecordingBluetoothMock(sharedPage);
    await sharedPage.goto(APP_URL, { waitUntil: 'domcontentloaded' });
    try {
      await waitForAppReady(sharedPage);
    } catch (loadErr) {
      // Dump page console logs so CI logs show what the WASM was doing before
      // the timeout.  This is the primary diagnostic for load failures.
      console.error('[diagnostic] waitForAppReady timed out. Page console output:');
      for (const entry of consoleLogs) {
        console.error(`  [${entry.type}] ${entry.text}`);
      }
      const pageTitle = await sharedPage.title().catch(() => '<unavailable>');
      const isSecure = await sharedPage.evaluate(() => window.isSecureContext).catch(() => '<unavailable>');
      const hasBt = await sharedPage.evaluate(() => typeof navigator.bluetooth).catch(() => '<unavailable>');
      const hasScan = await sharedPage.evaluate(() => typeof window.mt_startBleScan).catch(() => '<unavailable>');
      console.error(`[diagnostic] page title: ${pageTitle}, isSecureContext: ${isSecure}, typeof navigator.bluetooth: ${hasBt}, typeof mt_startBleScan: ${hasScan}`);
      throw loadErr;
    }
  }, { timeout: 420_000 }); // 7 min: WASM JIT-compilation takes 3+ min on cold CI runners

  test.afterAll(async () => {
    await sharedPage?.close();
    await sharedContext?.close();
  });

  // Reset the recording mock before each BLE test so every assertion starts
  // with a clean slate (all call arrays empty, requestDeviceFilters undefined).
  async function resetRecordingMock() {
    consoleLogs.length = 0;
    await sharedPage.evaluate(() => {
      window._btleApiCalls = {
        requestDeviceFilters: undefined,
        getPrimaryService:    [],
        getCharacteristic:    [],
        startNotifications:   [],
        writeValueWithResponse: []
      };
    });
  }

  // ── requestDevice service filters ──────────────────────────────────────────
  test('requestDevice is called with correct service filter UUIDs', async () => {
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
  });

  // ── getPrimaryService for each sensor profile ──────────────────────────────
  test('getPrimaryService is called for each sensor profile', async () => {
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
  });

  // ── startNotifications for each sensor characteristic ─────────────────────
  test('startNotifications is called for each sensor characteristic', async () => {
    await resetRecordingMock();
    await triggerBleScanAndWaitForRequestDevice(sharedPage);

    const recorded = await sharedPage.evaluate(() => window._btleApiCalls);

    // One startNotifications per characteristic in profileMap
    for (const charUuid of [0x2A37, 0x2A5B, 0x2A63, 0x2AD2]) {
      expect(recorded.startNotifications,
        `startNotifications was not called for 0x${charUuid.toString(16).toUpperCase()}`)
        .toContain(charUuid);
    }
  });

  // ── FTMS Request Control write ─────────────────────────────────────────────
  test('FTMS Request Control (opcode 0x00) is written to characteristic 0x2AD9', async () => {
    await resetRecordingMock();
    await triggerBleScanAndWaitForRequestDevice(sharedPage);
    await waitForFtmsWrite(sharedPage);

    const recorded = await sharedPage.evaluate(() => window._btleApiCalls);

    // js_requestFtmsControl writes [0x00] to FTMS Control Point (0x2AD9)
    const ftmsWrite = (recorded.writeValueWithResponse || [])
      .find(w => w.uuid === 0x2AD9 && w.bytes[0] === 0x00);

    expect(ftmsWrite,
      'writeValueWithResponse([0x00]) on FTMS Control Point 0x2AD9 was not called')
      .toBeDefined();
  });
});
