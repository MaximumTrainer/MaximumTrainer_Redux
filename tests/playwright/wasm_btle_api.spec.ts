import { test, expect } from '@playwright/test';
import { WasmAppPage, APP_URL } from './pages/WasmAppPage';

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

/** Screenshot output directory — uploaded as a CI artifact. */
const SCREENSHOT_DIR = 'test-results/wasm-screenshots';

// ─── Tests ────────────────────────────────────────────────────────────────────

test.describe('WASM BLE API — Web Bluetooth call verification', () => {
  // The WASM binary (~18 MB, ASYNCIFY-transformed) can take 2+ minutes to
  // JIT-compile on cold CI runners.  Loading it once in beforeAll and sharing
  // the page across all four tests avoids repeating that cost four times.
  //
  // test.describe.configure is the authoritative timeout override that
  // Playwright guarantees is applied to beforeAll hooks as well as individual
  // tests (the beforeAll { timeout } option is silently ignored in some
  // Playwright versions when no describe-level timeout is set).
  test.describe.configure({ timeout: 420_000 });

  let sharedWasmApp: WasmAppPage;
  let sharedContext: import('@playwright/test').BrowserContext;

  const consoleLogs:     Array<{ type: string; text: string }> = [];
  const pageErrors:      string[]                               = [];
  const failedRequests:  string[]                               = [];

  test.beforeAll(async ({ browser }) => {
    // test.setTimeout() inside beforeAll changes the currently-running hook's
    // deadline — the only approach that works reliably across all Playwright
    // versions when describe.configure is insufficient.
    test.setTimeout(420_000);

    sharedContext = await browser.newContext();
    const sharedPage = await sharedContext.newPage();
    sharedWasmApp = new WasmAppPage(sharedPage);

    sharedPage.on('console',       (msg) => consoleLogs.push({ type: msg.type(), text: msg.text() }));
    sharedPage.on('pageerror',     (err) => pageErrors.push(err.message));
    sharedPage.on('requestfailed', (req) =>
      failedRequests.push(`${req.url()} — ${req.failure()?.errorText}`),
    );

    await sharedWasmApp.injectRecordingBluetoothMock();

    // Mock maximumtrainer.com backend APIs before navigation so all XHR
    // requests from the Qt app return 200 instead of failing with CORS /
    // connection-refused errors that produce WARN overlay lines.
    const apiRequestedUrls = await sharedWasmApp.mockBackendApis();

    await sharedWasmApp.goto();

    try {
      await sharedWasmApp.waitForFullyLoaded();
    } catch (loadErr) {
      // Dump all available diagnostics so CI logs show exactly what happened.
      console.error('[diagnostic] waitForFullyLoaded timed out.');
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
      const page = sharedWasmApp.page;
      const ev = (fn: () => unknown) => page.evaluate(fn).catch((e: Error) => `<eval failed: ${e.message}>`);
      console.error('[diagnostic]',
        `isSecureContext=${await ev(() => window.isSecureContext)}`,
        `navigator.bluetooth=${await ev(() => typeof (navigator as any).bluetooth)}`,
        `mt_startBleScan=${await ev(() => typeof (window as any).mt_startBleScan)}`,
        `createQtAppInstance=${await ev(() => typeof (window as any).createQtAppInstance)}`,
      );
      const loadingText  = await ev(() => document.getElementById('loading-status')?.textContent);
      const loadingClass = await ev(() => document.getElementById('loading-screen')?.className);
      console.error(`[diagnostic] loading-screen class="${loadingClass}", status text="${loadingText}"`);
      throw loadErr;
    }

    // Capture settled app state screenshot (always saved regardless of pass/fail).
    await sharedWasmApp.page.screenshot({ path: `${SCREENSHOT_DIR}/wasm-app-ready.png` });

    // Assert that Qt's initialisation did not emit a 'number NaN' startup error.
    const overlayText = await sharedWasmApp.page.evaluate(() => {
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
    const fatalErrors = await sharedWasmApp.logOverlay.getFatalErrorLines();
    expect(fatalErrors,
      `WASM log overlay contains unexpected ERROR lines:\n${fatalErrors.join('\n')}`)
      .toHaveLength(0);

    // Assert no Qt network-error WARN lines.
    const notFoundLines = await sharedWasmApp.logOverlay.getLinesContaining('Not Found');
    expect(notFoundLines,
      `WASM overlay shows "Not Found" — backend API requests are failing:\n${notFoundLines.join('\n')}`)
      .toHaveLength(0);

    const statusCode0Lines = await sharedWasmApp.logOverlay.getLinesContaining('HTTP status code 0');
    expect(statusCode0Lines,
      `WASM overlay shows "HTTP status code 0" — backend API requests are CORS-blocked:\n${statusCode0Lines.join('\n')}`)
      .toHaveLength(0);

    const radioFetchFailedLines = await sharedWasmApp.logOverlay.getLinesContaining('Radio list fetch failed');
    expect(radioFetchFailedLines,
      `WASM overlay shows "Radio list fetch failed" — radio API request failed:\n${radioFetchFailedLines.join('\n')}`)
      .toHaveLength(0);

    // Verify the backend API endpoints were actually called.
    expect(apiRequestedUrls.some((u) => u.includes('radio_rest')),
      `Qt MainWindow did not call the radio API.\nSeen URLs: ${apiRequestedUrls.join(', ')}`)
      .toBe(true);
    expect(apiRequestedUrls.some((u) => u.includes('achievement_rest')),
      `Qt ManagerAchievement did not call the achievement API.\nSeen URLs: ${apiRequestedUrls.join(', ')}`)
      .toBe(true);
  });

  test.afterAll(async () => {
    await sharedContext?.close();
  });

  // Reset the recording mock before each BLE test so every assertion starts
  // with a clean slate (all call arrays empty, requestDeviceFilters undefined).
  test.beforeEach(async () => {
    consoleLogs.length    = 0;
    pageErrors.length     = 0;
    failedRequests.length = 0;
    await sharedWasmApp.resetRecordingMock();
  });

  // ── requestDevice service filters ──────────────────────────────────────────
  test('requestDevice is called with correct service filter UUIDs', async ({}, testInfo) => {
    await sharedWasmApp.triggerBleScanAndWaitForRequestDevice();

    const recorded = await sharedWasmApp.page.evaluate(
      () => (window as any)._btleApiCalls,
    );

    // requestDevice must have been called by js_scanAndConnect
    expect(recorded.requestDeviceFilters,
      'navigator.bluetooth.requestDevice() was not called — BLE scan was not initiated')
      .not.toBeNull();

    // Service filter UUIDs must include the four core BLE sensor profiles
    const filterServices: number[] = (recorded.requestDeviceFilters || [])
      .flatMap((f: { services?: number[] }) => f.services || []);

    for (const svcUuid of [0x180D, 0x1816, 0x1818, 0x1826]) {
      expect(filterServices,
        `requestDevice filter must include service 0x${svcUuid.toString(16).toUpperCase()}`)
        .toContain(svcUuid);
    }

    // No [MT] BLE error messages should appear during a successful mock connection
    const bleErrors = consoleLogs.filter(
      (m) => m.type === 'error' && m.text.includes('[MT]'),
    );
    expect(bleErrors,
      `Unexpected [MT] BLE errors: ${bleErrors.map((m) => m.text).join(', ')}`)
      .toHaveLength(0);

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await sharedWasmApp.page.screenshot({
      path: `${SCREENSHOT_DIR}/ble-api-request-device-filters${retrySuffix}.png`,
    });
  });

  // ── getPrimaryService for each sensor profile ──────────────────────────────
  test('getPrimaryService is called for each sensor profile', async ({}, testInfo) => {
    await sharedWasmApp.triggerBleScanAndWaitForRequestDevice();

    const recorded = await sharedWasmApp.page.evaluate(
      () => (window as any)._btleApiCalls,
    );

    // subscribeAll iterates over all services in profileMap; js_requestFtmsControl
    // also calls getPrimaryService(0x1826) separately.
    for (const svcUuid of [0x180D, 0x1816, 0x1818, 0x1826]) {
      expect(recorded.getPrimaryService,
        `getPrimaryService(0x${svcUuid.toString(16).toUpperCase()}) was not called`)
        .toContain(svcUuid);
    }

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await sharedWasmApp.page.screenshot({
      path: `${SCREENSHOT_DIR}/ble-api-get-primary-service${retrySuffix}.png`,
    });
  });

  // ── startNotifications for each sensor characteristic ─────────────────────
  test('startNotifications is called for each sensor characteristic', async ({}, testInfo) => {
    await sharedWasmApp.triggerBleScanAndWaitForRequestDevice();

    const recorded = await sharedWasmApp.page.evaluate(
      () => (window as any)._btleApiCalls,
    );

    // One startNotifications per characteristic in profileMap
    for (const charUuid of [0x2A37, 0x2A5B, 0x2A63, 0x2AD2]) {
      expect(recorded.startNotifications,
        `startNotifications was not called for 0x${charUuid.toString(16).toUpperCase()}`)
        .toContain(charUuid);
    }

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await sharedWasmApp.page.screenshot({
      path: `${SCREENSHOT_DIR}/ble-api-start-notifications${retrySuffix}.png`,
    });
  });

  // ── FTMS Request Control write ─────────────────────────────────────────────
  test('FTMS Request Control (opcode 0x00) is written to characteristic 0x2AD9', async ({}, testInfo) => {
    await sharedWasmApp.triggerBleScanAndWaitForRequestDevice();

    // BtleHubWasm is only instantiated when a workout runs, so g_connectedCallback
    // is never set during Playwright CI.  Use the dedicated test hook
    // mt_requestFtmsControl() which invokes js_requestFtmsControl() directly.
    const hasFtmsHook = await sharedWasmApp.page.evaluate(
      () => typeof (window as any).mt_requestFtmsControl === 'function',
    );
    expect(hasFtmsHook,
      'window.mt_requestFtmsControl hook absent — WASM build must be rebuilt with updated js_exposeTestScanApi')
      .toBeTruthy();
    await sharedWasmApp.page.evaluate(() => (window as any).mt_requestFtmsControl());
    await sharedWasmApp.waitForFtmsWrite();

    const recorded = await sharedWasmApp.page.evaluate(
      () => (window as any)._btleApiCalls,
    );

    // js_requestFtmsControl writes [0x00] to FTMS Control Point (0x2AD9)
    const ftmsWrite = ((recorded.writeValueWithResponse as Array<{ uuid: number; bytes: number[] }>) || [])
      .find((w) => w.uuid === 0x2AD9 && w.bytes[0] === 0x00);

    expect(ftmsWrite,
      'writeValueWithResponse([0x00]) on FTMS Control Point 0x2AD9 was not called')
      .toBeDefined();

    const retrySuffix = testInfo.retry > 0 ? `-retry${testInfo.retry}` : '';
    await sharedWasmApp.page.screenshot({
      path: `${SCREENSHOT_DIR}/ble-api-ftms-request-control${retrySuffix}.png`,
    });
  });
});
