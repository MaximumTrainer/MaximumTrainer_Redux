// @ts-check
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

const { test, expect } = require('@playwright/test');
const {
  stubBluetooth,
  waitForAppReady,
  mockBackendApis,
  mockIntervalsIcuApi,
  getOverlayFatalErrorLines,
} = require('./wasm-test-helpers');

const BASE_ORIGIN = process.env.PLAYWRIGHT_BASE_URL || 'https://maximumtrainer.github.io/MaximumTrainer_Redux';
const APP_URL = `${BASE_ORIGIN}/app/`;

// Fake credentials used only for Playwright route mocking — not real secrets.
const TEST_ATHLETE_ID = 'i00000';
const TEST_API_KEY    = 'playwright-test-key';

// ─────────────────────────────────────────────────────────────────────────────

test.describe('Intervals.icu WASM functional', () => {
  // The WASM binary (~18 MB, ASYNCIFY-transformed) takes 2+ minutes to
  // JIT-compile on cold CI runners.  We use two shared-page groups to avoid
  // reloading the app multiple times and to stay within the 7-minute budget.
  //
  // Group A (no credentials): tests 1 & 2 share one page load.
  // Group B (with credentials): tests 3 & 4 share one page load.
  //
  // test.describe.configure sets 7 min per test (matches btle suite).
  // test.setTimeout(420_000) inside beforeAll ensures the hook itself
  // doesn't time out while waiting for the WASM to initialise.
  test.describe.configure({ timeout: 420_000 });

  // ── Group A: No credentials ────────────────────────────────────────────────
  test.describe('without credentials', () => {
    test.describe.configure({ timeout: 420_000 });

    /** @type {import('@playwright/test').Page} */
    let page;
    /** @type {import('@playwright/test').BrowserContext} */
    let ctx;
    let intervalsRequests = [];

    test.beforeAll(async ({ browser }) => {
      test.setTimeout(420_000);
      ctx  = await browser.newContext();
      page = await ctx.newPage();
      await stubBluetooth(page);
      await mockBackendApis(page);
      intervalsRequests = await mockIntervalsIcuApi(page);
      await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
      await waitForAppReady(page, 300_000);
      // Give the app a moment to fire any startup network requests.
      await page.waitForTimeout(5_000);
    });

    test.afterAll(async () => { await ctx.close(); });

    test('no API calls to intervals.icu before credentials are set', async () => {
      const dataRequests = intervalsRequests.filter(r => !r.startsWith('OPTIONS'));
      expect(
        dataRequests,
        `App made unexpected data requests to intervals.icu on startup (no credentials set): ${dataRequests.join(', ')}`,
      ).toHaveLength(0);
    });

    test('mt_intervalsRefresh test hook is exposed after app loads', async () => {
      const hookExists = await page.evaluate(() => typeof window.mt_intervalsRefresh === 'function');
      expect(
        hookExists,
        'window.mt_intervalsRefresh should be a function after the WASM app loads',
      ).toBe(true);
    });
  });

  // ── Group B: With mock credentials ────────────────────────────────────────
  test.describe('with mock credentials', () => {
    test.describe.configure({ timeout: 420_000 });

    /** @type {import('@playwright/test').Page} */
    let page;
    /** @type {import('@playwright/test').BrowserContext} */
    let ctx;
    let intervalsRequests = [];

    test.beforeAll(async ({ browser }) => {
      test.setTimeout(420_000);
      ctx  = await browser.newContext();
      page = await ctx.newPage();
      await stubBluetooth(page);
      await mockBackendApis(page);
      intervalsRequests = await mockIntervalsIcuApi(page);

      await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
      await waitForAppReady(page, 300_000);

      // Wait for BOTH test hooks to be registered by the C++ WASM code.
      await page.waitForFunction(
        () => typeof window.mt_setIntervalsCredentials === 'function' &&
              typeof window.mt_intervalsRefresh === 'function',
        null,
        { timeout: 120_000 },
      );

      // Inject credentials directly into the Account object (bypasses QSettings
      // localStorage format differences across Qt versions / browsers).
      await page.evaluate(
        ({ apiKey, athleteId }) => window.mt_setIntervalsCredentials(apiKey, athleteId),
        { apiKey: TEST_API_KEY, athleteId: TEST_ATHLETE_ID },
      );

      // Set up request listener BEFORE triggering the refresh so we don't miss
      // a fast response.  Signal-based wait is more reliable than a fixed sleep.
      const calendarRequestPromise = page.waitForRequest(
        req => req.url().includes('intervals.icu') && req.url().includes('/events'),
        { timeout: 30_000 },
      );

      // Trigger calendar refresh.
      await page.evaluate(() => window.mt_intervalsRefresh());

      // Wait for the actual network request (Qt::QueuedConnection fires on the
      // next event-loop tick, so this should resolve almost immediately).
      await calendarRequestPromise;
    });

    test.afterAll(async () => { await ctx.close(); });

    test('calendar fetch is triggered when credentials are set and refresh hook is called', async () => {
      const calendarRequests = intervalsRequests.filter(r => r.includes('/events'));
      expect(
        calendarRequests.length,
        `Expected at least one calendar fetch request after mt_intervalsRefresh(). ` +
        `All intercepted requests: ${intervalsRequests.join(', ')}`,
      ).toBeGreaterThan(0);
    });

    test('no WASM ERROR log entries after calendar fetch with mocked API', async () => {
      const errorLines = await getOverlayFatalErrorLines(page);
      expect(
        errorLines,
        `Unexpected WASM ERROR log entries after calendar fetch: ${errorLines.join('; ')}`,
      ).toHaveLength(0);
    });
  });
});
