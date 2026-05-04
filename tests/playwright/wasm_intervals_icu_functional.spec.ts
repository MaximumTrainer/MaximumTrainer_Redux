/**
 * Functional WASM tests for the Intervals.icu calendar tab.
 *
 * These tests verify the full end-to-end flow in the browser:
 *   1. No API calls before credentials are set
 *   2. Calendar refreshes when triggered via the test hook
 *   3. No WASM ERROR log entries after a successful (mocked) fetch
 *
 * The Intervals.icu API is mocked via Playwright route interception so the
 * tests run offline and never hit the real API.  CORS preflight (OPTIONS)
 * requests are handled so the browser's CORS enforcement is satisfied.
 *
 * NOTE: These tests exercise functionality that requires the WASM binary to be
 * rebuilt with the intervals.icu WASM guards removed.  They will fail against
 * an older pre-built binary and pass after a fresh CI build.
 */

import { test, expect } from '@playwright/test';
import { WasmAppPage, APP_URL } from './pages/WasmAppPage';

// Fake credentials used only for Playwright route mocking — not real secrets.
const TEST_ATHLETE_ID = 'i00000';
const TEST_API_KEY    = 'playwright-test-key';

// ─────────────────────────────────────────────────────────────────────────────

test.describe('Intervals.icu WASM functional', () => {
  // The WASM binary (~18 MB, ASYNCIFY-transformed) takes 2+ minutes to
  // JIT-compile on cold CI runners.  We use two shared-page groups to avoid
  // reloading the app multiple times and to stay within the 7-minute budget.
  test.describe.configure({ timeout: 420_000 });

  // ── Group A: No credentials ────────────────────────────────────────────────
  test.describe('without credentials', () => {
    test.describe.configure({ timeout: 420_000 });

    let wasmApp: WasmAppPage;
    let ctx: import('@playwright/test').BrowserContext;
    let intervalsRequests: string[] = [];

    test.beforeAll(async ({ browser }) => {
      test.setTimeout(420_000);
      ctx = await browser.newContext();
      wasmApp = new WasmAppPage(await ctx.newPage());

      await wasmApp.stubBluetooth();
      await wasmApp.mockBackendApis();
      intervalsRequests = await wasmApp.mockIntervalsIcuApi();

      await wasmApp.goto();
      await wasmApp.waitForFullyLoaded(300_000);
      // Give the app a moment to fire any startup network requests.
      await wasmApp.page.waitForTimeout(5_000);
    });

    test.afterAll(async () => { await ctx.close(); });

    test('no API calls to intervals.icu before credentials are set', async () => {
      const dataRequests = intervalsRequests.filter((r) => !r.startsWith('OPTIONS'));
      expect(
        dataRequests,
        `App made unexpected data requests to intervals.icu on startup (no credentials set): ${dataRequests.join(', ')}`,
      ).toHaveLength(0);
    });

    test('mt_intervalsRefresh test hook is exposed after app loads', async () => {
      const hookExists = await wasmApp.page.evaluate(
        () => typeof (window as any).mt_intervalsRefresh === 'function',
      );
      expect(
        hookExists,
        'window.mt_intervalsRefresh should be a function after the WASM app loads',
      ).toBe(true);
    });
  });

  // ── Group B: With mock credentials ────────────────────────────────────────
  test.describe('with mock credentials', () => {
    test.describe.configure({ timeout: 420_000 });

    let wasmApp: WasmAppPage;
    let ctx: import('@playwright/test').BrowserContext;
    let intervalsRequests: string[] = [];

    test.beforeAll(async ({ browser }) => {
      test.setTimeout(420_000);
      ctx = await browser.newContext();
      wasmApp = new WasmAppPage(await ctx.newPage());

      await wasmApp.stubBluetooth();
      await wasmApp.mockBackendApis();
      intervalsRequests = await wasmApp.mockIntervalsIcuApi();

      await wasmApp.goto();
      await wasmApp.waitForFullyLoaded(300_000);

      // Wait for BOTH test hooks to be registered by the C++ WASM code.
      await wasmApp.page.waitForFunction(
        () =>
          typeof (window as any).mt_setIntervalsCredentials === 'function' &&
          typeof (window as any).mt_intervalsRefresh === 'function',
        null,
        { timeout: 120_000 },
      );

      // Inject credentials directly into the Account object.
      await wasmApp.page.evaluate(
        ({ apiKey, athleteId }: { apiKey: string; athleteId: string }) =>
          (window as any).mt_setIntervalsCredentials(apiKey, athleteId),
        { apiKey: TEST_API_KEY, athleteId: TEST_ATHLETE_ID },
      );

      // Set up response listener BEFORE triggering refresh to avoid a race.
      const calendarResponsePromise = wasmApp.page.waitForResponse(
        (resp) =>
          resp.url().includes('intervals.icu') && resp.url().includes('/events'),
        { timeout: 30_000 },
      );

      await wasmApp.page.evaluate(() => (window as any).mt_intervalsRefresh());
      await calendarResponsePromise;
    });

    test.afterAll(async () => { await ctx.close(); });

    test('calendar fetch is triggered when credentials are set and refresh hook is called', async () => {
      const calendarRequests = intervalsRequests.filter((r) => r.includes('/events'));
      expect(
        calendarRequests.length,
        `Expected at least one calendar fetch request after mt_intervalsRefresh(). ` +
        `All intercepted requests: ${intervalsRequests.join(', ')}`,
      ).toBeGreaterThan(0);
    });

    test('no WASM ERROR log entries after calendar fetch with mocked API', async () => {
      const errorLines = await wasmApp.logOverlay.getFatalErrorLines();
      expect(
        errorLines,
        `Unexpected WASM ERROR log entries after calendar fetch: ${errorLines.join('; ')}`,
      ).toHaveLength(0);
    });
  });
});
