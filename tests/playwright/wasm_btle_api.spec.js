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

// Wait for the Qt canvas to become visible (app fully initialised).
// The onLoaded callback in index.html sets style.visibility = 'visible' explicitly.
// The canvas element has no inline style initially (only CSS visibility:hidden),
// so checking !== 'hidden' would be trivially true from page load; we need === 'visible'.
async function waitForAppReady(page) {
  await page.waitForFunction(
    () => {
      const canvas = document.querySelector('#qt-canvas-wrapper');
      return canvas && canvas.style.visibility === 'visible';
    },
    { timeout: 45000 }
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
    { timeout: 45000 }
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
    { timeout: 45000 }
  );
}

// ─── Tests ────────────────────────────────────────────────────────────────────

test.describe('WASM BLE API — Web Bluetooth call verification', () => {

  // These tests load the deployed WASM app and trigger a full BLE scan + GATT
  // setup flow.  The WASM binary can take 30–50 s to load and initialise on
  // CI runners, so give each test 120 s (2×default) instead of the global 60 s.
  test.describe.configure({ timeout: 120_000 });

  // ── requestDevice service filters ──────────────────────────────────────────
  test('requestDevice is called with correct service filter UUIDs', async ({ page }) => {
    await injectRecordingBluetoothMock(page);

    const consoleLogs = [];
    page.on('console', msg => consoleLogs.push({ type: msg.type(), text: msg.text() }));

    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
    await waitForAppReady(page);
    await triggerBleScanAndWaitForRequestDevice(page);

    const recorded = await page.evaluate(() => window._btleApiCalls);

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
  test('getPrimaryService is called for each sensor profile', async ({ page }) => {
    await injectRecordingBluetoothMock(page);
    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
    await waitForAppReady(page);
    await triggerBleScanAndWaitForRequestDevice(page);

    const recorded = await page.evaluate(() => window._btleApiCalls);

    // subscribeAll iterates over all services in profileMap; js_requestFtmsControl
    // also calls getPrimaryService(0x1826) separately.
    for (const svcUuid of [0x180D, 0x1816, 0x1818, 0x1826]) {
      expect(recorded.getPrimaryService,
        `getPrimaryService(0x${svcUuid.toString(16).toUpperCase()}) was not called`)
        .toContain(svcUuid);
    }
  });

  // ── startNotifications for each sensor characteristic ─────────────────────
  test('startNotifications is called for each sensor characteristic', async ({ page }) => {
    await injectRecordingBluetoothMock(page);
    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
    await waitForAppReady(page);
    await triggerBleScanAndWaitForRequestDevice(page);

    const recorded = await page.evaluate(() => window._btleApiCalls);

    // One startNotifications per characteristic in profileMap
    for (const charUuid of [0x2A37, 0x2A5B, 0x2A63, 0x2AD2]) {
      expect(recorded.startNotifications,
        `startNotifications was not called for 0x${charUuid.toString(16).toUpperCase()}`)
        .toContain(charUuid);
    }
  });

  // ── FTMS Request Control write ─────────────────────────────────────────────
  test('FTMS Request Control (opcode 0x00) is written to characteristic 0x2AD9', async ({ page }) => {
    await injectRecordingBluetoothMock(page);
    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
    await waitForAppReady(page);
    await triggerBleScanAndWaitForRequestDevice(page);
    await waitForFtmsWrite(page);

    const recorded = await page.evaluate(() => window._btleApiCalls);

    // js_requestFtmsControl writes [0x00] to FTMS Control Point (0x2AD9)
    const ftmsWrite = (recorded.writeValueWithResponse || [])
      .find(w => w.uuid === 0x2AD9 && w.bytes[0] === 0x00);

    expect(ftmsWrite,
      'writeValueWithResponse([0x00]) on FTMS Control Point 0x2AD9 was not called')
      .toBeDefined();
  });
});
