import { type Page } from '@playwright/test';
import { LoadingScreenWidget }        from '../widgets/LoadingScreenWidget';
import { BrowserCompatibilityWidget } from '../widgets/BrowserCompatibilityWidget';
import { BleReconnectWidget }         from '../widgets/BleReconnectWidget';
import { LogOverlayWidget }           from '../widgets/LogOverlayWidget';

const BASE_ORIGIN =
  process.env['PLAYWRIGHT_BASE_URL'] ||
  'https://maximumtrainer.github.io/MaximumTrainer_Redux';

/** Full URL of the WASM app shell page. */
export const APP_URL  = `${BASE_ORIGIN}/app/`;
/** Base directory URL of the deployed app (no trailing slash). */
export const BASE_URL = `${BASE_ORIGIN}/app`;

/**
 * Page Object for the MaximumTrainer WASM web application.
 *
 * The Qt application renders into a `<canvas id="qt-canvas">` element and
 * cannot be inspected with standard DOM locators.  All interaction with Qt
 * UI components must go through JS-bridge test hooks
 * (`window.mt_startBleScan`, `window.mt_setIntervalsCredentials`, …)
 * that are registered by the C++ WASM code at initialisation time.
 *
 * The HTML shell page (`docs/app/index.html`) exposes several first-class
 * DOM elements (loading screen, compatibility warning, BLE reconnect overlay,
 * log overlay) that *can* be tested with standard Playwright locators — each
 * is encapsulated as a typed Widget property on this class.
 *
 * ## OS / platform notes
 * - All keyboard shortcuts in Qt are handled inside the WASM canvas and are
 *   not accessible via `page.keyboard` in a meaningful way.
 * - Clipboard operations (`Ctrl+C` / `Cmd+C`) depend on browser permissions
 *   and differ across Linux/macOS/Windows; avoid testing clipboard content.
 * - The WASM binary requires Chromium; Firefox and WebKit lack Web Bluetooth.
 */
export class WasmAppPage {
  readonly page: Page;

  // ── Widgets ──────────────────────────────────────────────────────────────

  /** Loading progress screen (`#loading-screen`) */
  readonly loadingScreen: LoadingScreenWidget;
  /** Browser compatibility warning banner (`#browser-warning`) */
  readonly browserCompat: BrowserCompatibilityWidget;
  /** BLE trainer reconnect overlay (`#ble-reconnect-overlay`) */
  readonly bleReconnect: BleReconnectWidget;
  /** Diagnostic WASM log overlay (`#wasm-log-overlay`) */
  readonly logOverlay: LogOverlayWidget;

  constructor(page: Page) {
    this.page          = page;
    this.loadingScreen = new LoadingScreenWidget(page);
    this.browserCompat = new BrowserCompatibilityWidget(page);
    this.bleReconnect  = new BleReconnectWidget(page);
    this.logOverlay    = new LogOverlayWidget(page);
  }

  // ── Navigation ───────────────────────────────────────────────────────────

  /** Navigate to the WASM app shell page and wait for DOM content load. */
  async goto(): Promise<void> {
    await this.page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
  }

  // ── Bluetooth stub helpers ────────────────────────────────────────────────

  /**
   * Inject a stub `navigator.bluetooth` that passes the capability check
   * (`getAvailability` resolves `true`) but throws on `requestDevice`.
   * Prevents the compatibility warning from appearing.
   *
   * **Must be called before `goto()`.**
   */
  async stubBluetooth(): Promise<void> {
    await this.page.addInitScript(() => {
      if (!(navigator as any).bluetooth) {
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

  /**
   * Stub `navigator.bluetooth` with `getAvailability` returning `false`.
   * Triggers the "No Bluetooth adapter detected" compatibility warning.
   *
   * **Must be called before `goto()`.**
   */
  async stubBluetoothUnavailable(): Promise<void> {
    await this.page.addInitScript(() => {
      Object.defineProperty(navigator, 'bluetooth', {
        value: {
          requestDevice: async () => { throw new Error('stub'); },
          getAvailability: async () => false,
        },
        configurable: true,
      });
    });
  }

  /**
   * Remove `navigator.bluetooth` entirely to simulate an unsupported browser
   * (e.g. Firefox without the `dom.bluetooth.enabled` flag).
   * Triggers the "Web Bluetooth API is not available" compatibility warning.
   *
   * **Must be called before `goto()`.**
   */
  async stubBluetoothAbsent(): Promise<void> {
    await this.page.addInitScript(() => {
      Object.defineProperty(navigator, 'bluetooth', {
        value: undefined,
        configurable: true,
      });
    });
  }

  /**
   * Inject a recording Web Bluetooth mock that resolves all GATT calls
   * successfully and captures every service/characteristic UUID requested.
   *
   * After the app loads, call results are accessible via
   * `page.evaluate(() => window._btleApiCalls)`.
   *
   * **Must be called before `goto()`.**
   */
  async injectRecordingBluetoothMock(): Promise<void> {
    await this.page.addInitScript(() => {
      // Intercept logger.js's unconditional assignment of window._wasmLogger so
      // that its log() calls are also forwarded to the browser console (and thus
      // captured by Playwright's `page.on('console')` handler).
      // A plain assignment here would be overwritten when logger.js loads;
      // a defineProperty setter fires on that overwrite and wraps the impl.
      Object.defineProperty(window, '_wasmLogger', {
        configurable: true,
        set(impl: { log: (msg: string) => void }) {
          const origLog = impl.log.bind(impl);
          impl.log = (msg: string) => { console.log('[wasm-load] ' + msg); return origLog(msg); };
          Object.defineProperty(window, '_wasmLogger', {
            configurable: true, writable: true, value: impl,
          });
        },
        get() { return undefined; },
      });

      const calls: {
        requestDeviceFilters: any;
        getPrimaryService:     any[];
        getCharacteristic:     any[];
        startNotifications:    any[];
        writeValueWithResponse: Array<{ uuid: any; bytes: number[] }>;
      } = (window as any)._btleApiCalls = {
        requestDeviceFilters:  undefined,
        getPrimaryService:     [],
        getCharacteristic:     [],
        startNotifications:    [],
        writeValueWithResponse: [],
      };

      const makeChar = (charUuid: any) => ({
        startNotifications: async () => { calls.startNotifications.push(charUuid); },
        addEventListener:   () => {},
        writeValueWithResponse: async (bytes: ArrayBuffer) => {
          calls.writeValueWithResponse.push({
            uuid:  charUuid,
            bytes: Array.from(new Uint8Array(bytes)),
          });
        },
      });

      const makeService = (_svcUuid: any) => ({
        getCharacteristic: async (charUuid: any) => {
          calls.getCharacteristic.push(charUuid);
          return makeChar(charUuid);
        },
      });

      const mockGattServer: any = {
        connected:         true,
        connect:           async () => { mockGattServer.connected = true; return mockGattServer; },
        getPrimaryService: async (svcUuid: any) => {
          calls.getPrimaryService.push(svcUuid);
          return makeService(svcUuid);
        },
        disconnect: () => { mockGattServer.connected = false; },
      };

      const mockDevice = {
        name:             'MockBleTrainer',
        gatt:             mockGattServer,
        addEventListener: () => {},
      };

      Object.defineProperty(navigator, 'bluetooth', {
        value: {
          requestDevice: async (options: any) => {
            calls.requestDeviceFilters =
              options && options.filters ? options.filters : null;
            return mockDevice;
          },
          getAvailability: async () => true,
        },
        configurable: true,
      });
    });
  }

  // ── Network mock helpers ──────────────────────────────────────────────────

  /**
   * Register Playwright route intercepts for all `maximumtrainer.com` backend
   * API endpoints.  Both `radio_rest` and `achievement_rest` return an empty
   * JSON array `[]`; all other paths return `{}`.
   *
   * Returns a live array of intercepted `"METHOD URL"` strings that is
   * populated as requests arrive.
   *
   * **Must be called before `goto()`.**
   */
  async mockBackendApis(): Promise<string[]> {
    const requestedUrls: string[] = [];

    await this.page.route('https://maximumtrainer.com/**', async (route) => {
      const url    = route.request().url();
      const method = route.request().method();
      requestedUrls.push(`${method} ${url}`);

      if (method === 'OPTIONS') {
        await route.fulfill({
          status: 204,
          headers: {
            'Access-Control-Allow-Origin':  '*',
            'Access-Control-Allow-Methods': 'GET, POST, PUT, DELETE, OPTIONS',
            'Access-Control-Allow-Headers': '*',
          },
        });
      } else if (url.includes('radio_rest') || url.includes('achievement_rest')) {
        await route.fulfill({
          status: 200,
          headers: { 'Access-Control-Allow-Origin': '*', 'Content-Type': 'application/json' },
          body: '[]',
        });
      } else {
        await route.fulfill({
          status: 200,
          headers: { 'Access-Control-Allow-Origin': '*', 'Content-Type': 'application/json' },
          body: '{}',
        });
      }
    });

    return requestedUrls;
  }

  /**
   * Register Playwright route intercepts for `intervals.icu` API endpoints,
   * including CORS preflight (`OPTIONS`) support.
   *
   * `/events` returns a minimal calendar event array; `.zwo` workout files
   * return a minimal ZWO XML document; all other routes return `{}`.
   *
   * Returns a live array of intercepted `"METHOD URL"` strings.
   *
   * **Must be called before `goto()`.**
   */
  async mockIntervalsIcuApi(): Promise<string[]> {
    const requestedUrls: string[] = [];

    await this.page.route('https://intervals.icu/**', async (route) => {
      const url    = route.request().url();
      const method = route.request().method();
      requestedUrls.push(`${method} ${url}`);

      const corsHeaders = {
        'Access-Control-Allow-Origin':  '*',
        'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
        'Access-Control-Allow-Headers': 'authorization, content-type',
      };

      if (method === 'OPTIONS') {
        await route.fulfill({ status: 204, headers: corsHeaders });
        return;
      }

      if (url.includes('/events')) {
        await route.fulfill({
          status: 200,
          headers: { ...corsHeaders, 'Content-Type': 'application/json' },
          body: JSON.stringify([
            {
              id:               'evt001',
              name:             'Playwright Test Workout',
              start_date_local: new Date().toISOString().split('T')[0],
              type:             'Ride',
              moving_time:      3600,
              workout_id:       'wk001',
              description:      'Test workout for Playwright',
            },
          ]),
        });
      } else if (url.includes('/workouts/') && url.endsWith('.zwo')) {
        await route.fulfill({
          status: 200,
          headers: { ...corsHeaders, 'Content-Type': 'application/xml' },
          body: '<?xml version="1.0"?><workout_file><name>Playwright Test</name><workout><SteadyState Duration="60" Power="0.5"/></workout></workout_file>',
        });
      } else {
        await route.fulfill({
          status: 200,
          headers: { ...corsHeaders, 'Content-Type': 'application/json' },
          body: '{}',
        });
      }
    });

    return requestedUrls;
  }

  // ── App readiness helpers ─────────────────────────────────────────────────

  /**
   * Wait until the WASM Qt application has finished loading.
   *
   * Two signals are monitored — both are set in the `onLoaded` callback
   * inside `index.html`:
   *   1. `#loading-screen` gains the `hidden` CSS class.
   *   2. `#qt-canvas-wrapper` computed visibility becomes `visible`.
   *
   * A 3-second settling window after either signal lets Qt's `main()`
   * start up and surface any initialisation errors before assertions run.
   *
   * @param timeoutMs  Maximum wait in ms (default 300 s, generous for cold
   *                   CI runners that JIT-compile the ~18 MB WASM binary).
   */
  async waitForFullyLoaded(timeoutMs = 300_000): Promise<void> {
    await this.page.waitForFunction(
      () => {
        const screen = document.querySelector('#loading-screen');
        if (screen && screen.classList.contains('hidden')) return true;
        const wrapper = document.querySelector('#qt-canvas-wrapper');
        return !!(wrapper && window.getComputedStyle(wrapper).visibility === 'visible');
      },
      null,
      { timeout: timeoutMs },
    );
    await this.page.waitForTimeout(3_000);
  }

  // ── BLE interaction helpers ───────────────────────────────────────────────

  /**
   * Trigger a BLE scan via the `window.mt_startBleScan` JS-bridge test hook
   * registered by the C++ WASM bridge, then wait until
   * `navigator.bluetooth.requestDevice` has been called by the mock.
   *
   * Requires `injectRecordingBluetoothMock()` to have been called first.
   *
   * @throws If the `mt_startBleScan` hook is not exposed by the app build.
   */
  async triggerBleScanAndWaitForRequestDevice(): Promise<void> {
    const hasHook = await this.page.evaluate(
      () => typeof (window as any).mt_startBleScan === 'function',
    );
    if (!hasHook) {
      throw new Error(
        'window.mt_startBleScan was not registered by the app build — ' +
        'ensure the WASM binary was built with the test scan API enabled.',
      );
    }

    await this.page.evaluate(() => (window as any).mt_startBleScan());

    await this.page.waitForFunction(
      () => {
        const calls = (window as any)._btleApiCalls;
        return calls && calls.requestDeviceFilters !== undefined;
      },
      null,
      { timeout: 45_000 },
    );
  }

  /**
   * Wait until the full async GATT setup chain has completed.
   *
   * The last step recorded by the mock is `writeValueWithResponse` on the
   * FTMS Control Point (0x2AD9), which only fires after `subscribeAll` and
   * `requestFtmsControl` both finish.  Polling for it is more reliable than
   * a fixed delay.
   *
   * Requires `injectRecordingBluetoothMock()` to have been called first.
   */
  async waitForFtmsWrite(): Promise<void> {
    await this.page.waitForFunction(
      () => {
        const calls = (window as any)._btleApiCalls;
        return (
          calls &&
          Array.isArray(calls.writeValueWithResponse) &&
          calls.writeValueWithResponse.length > 0
        );
      },
      null,
      { timeout: 45_000 },
    );
  }

  /**
   * Reset the recording BLE mock call arrays in-place to a clean state.
   *
   * Mutates the **existing** `window._btleApiCalls` object rather than
   * replacing it — the mock's `requestDevice` closure captured the original
   * `calls` reference at init-script time; replacing the object would cause
   * `waitForFunction` to read from a stale reference.
   *
   * Call before each BLE assertion block to avoid cross-test contamination
   * when tests share a single page instance.
   */
  async resetRecordingMock(): Promise<void> {
    await this.page.evaluate(() => {
      const c = (window as any)._btleApiCalls;
      c.requestDeviceFilters   = undefined;
      c.getPrimaryService      = [];
      c.getCharacteristic      = [];
      c.startNotifications     = [];
      c.writeValueWithResponse = [];
    });
  }

  // ── Intervals.icu helpers ────────────────────────────────────────────────

  /**
   * Wait until both `mt_setIntervalsCredentials` and `mt_intervalsRefresh`
   * test hooks have been registered by the WASM app.
   *
   * These hooks are exposed in `main()` (after Qt initialisation), so calling
   * this method after `waitForFullyLoaded()` is sufficient.
   *
   * @param timeoutMs Maximum wait in ms (default 120 s).
   */
  async waitForIntervalsTestHooks(timeoutMs = 120_000): Promise<void> {
    await this.page.waitForFunction(
      () =>
        typeof (window as any).mt_setIntervalsCredentials === 'function' &&
        typeof (window as any).mt_intervalsRefresh === 'function',
      null,
      { timeout: timeoutMs },
    );
  }

  /**
   * Inject Intervals.icu credentials into the running WASM app via the
   * `mt_setIntervalsCredentials(apiKey, athleteId)` C++ test hook.
   *
   * This passes credentials directly to the C++ layer without writing them
   * to `localStorage` in the test, avoiding clear-text storage of sensitive
   * data in the browser.
   *
   * Requires `waitForIntervalsTestHooks()` to have been called (or awaited)
   * before this method.
   *
   * @param apiKey    Intervals.icu API key.
   * @param athleteId Intervals.icu athlete ID (e.g. `'i00000'`).
   */
  async injectIntervalsCredentials(apiKey: string, athleteId: string): Promise<void> {
    await this.page.evaluate(
      ({ key, id }: { key: string; id: string }) =>
        (window as any).mt_setIntervalsCredentials(key, id),
      { key: apiKey, id: athleteId },
    );
  }

  /**
   * Trigger an Intervals.icu calendar refresh via the `mt_intervalsRefresh`
   * C++ test hook.
   *
   * Requires `waitForIntervalsTestHooks()` to have been called (or awaited)
   * before this method.
   */
  async triggerIntervalsRefresh(): Promise<void> {
    await this.page.evaluate(() => (window as any).mt_intervalsRefresh());
  }

  // ── Convenience getters ───────────────────────────────────────────────────

  /** Full URL of the WASM app shell page. */
  get url(): string {
    return APP_URL;
  }

  /** Base URL of the deployed app directory (no trailing slash). */
  get baseUrl(): string {
    return BASE_URL;
  }
}
