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

// Qt WASM QSettings key format (Qt 6.5+ WebLocalStorageFormat):
// <org>/<app>/<group>/<key>
const QSETTINGS_PREFIX = 'Max++ inc./MaximumTrainer';

// Fake credentials used only for Playwright route mocking — not real secrets.
const TEST_ATHLETE_ID = 'i00000';
const TEST_API_KEY    = 'playwright-test-key';

// ─────────────────────────────────────────────────────────────────────────────

test.describe('Intervals.icu WASM functional', () => {
  // The WASM binary is large (~18 MB) and takes 2+ minutes to JIT on cold CI
  // runners.  A single shared 5-minute suite timeout covers all tests.
  test.describe.configure({ timeout: 300_000 });

  test('no API calls to intervals.icu before credentials are set', async ({ page }) => {
    await stubBluetooth(page);
    await mockBackendApis(page);
    const intervalsRequests = await mockIntervalsIcuApi(page);

    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
    await waitForAppReady(page);
    // Give the app an extra moment to fire any startup network requests.
    await page.waitForTimeout(5_000);

    // Filter out CORS preflights — only assert on real data requests.
    const dataRequests = intervalsRequests.filter(r => !r.startsWith('OPTIONS'));
    expect(
      dataRequests,
      `App made unexpected data requests to intervals.icu on startup (no credentials set): ${dataRequests.join(', ')}`,
    ).toHaveLength(0);
  });

  test('mt_intervalsRefresh test hook is exposed after app loads', async ({ page }) => {
    await stubBluetooth(page);
    await mockBackendApis(page);
    await mockIntervalsIcuApi(page);

    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
    await waitForAppReady(page);

    const hookExists = await page.evaluate(() => typeof window.mt_intervalsRefresh === 'function');
    expect(
      hookExists,
      'window.mt_intervalsRefresh should be a function after the WASM app loads',
    ).toBe(true);
  });

  test('calendar fetch is triggered when credentials are set and refresh hook is called', async ({ page }) => {
    await stubBluetooth(page);
    await mockBackendApis(page);
    const intervalsRequests = await mockIntervalsIcuApi(page);

    // Inject mock credentials into Qt WASM QSettings localStorage before app loads.
    await page.addInitScript(({ prefix, athleteId, apiKey }) => {
      try {
        localStorage.setItem(`${prefix}/account/intervals_icu_api_key`,     apiKey);
        localStorage.setItem(`${prefix}/account/intervals_icu_athlete_id`,  athleteId);
        localStorage.setItem(`${prefix}/account/intervals_icu_auto_upload`, 'false');
      } catch (e) {
        console.warn('localStorage injection failed:', e);
      }
    }, { prefix: QSETTINGS_PREFIX, athleteId: TEST_ATHLETE_ID, apiKey: TEST_API_KEY });

    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
    await waitForAppReady(page);

    // Wait for the test hook to become available.
    await page.waitForFunction(
      () => typeof window.mt_intervalsRefresh === 'function',
      null,
      { timeout: 30_000 },
    );

    // Trigger the calendar refresh via the test hook.
    await page.evaluate(() => window.mt_intervalsRefresh());

    // Allow time for the async network request to be made and intercepted.
    await page.waitForTimeout(5_000);

    const calendarRequests = intervalsRequests.filter(r => r.includes('/events'));
    expect(
      calendarRequests.length,
      `Expected at least one calendar fetch request after mt_intervalsRefresh(). ` +
      `All intercepted requests: ${intervalsRequests.join(', ')}`,
    ).toBeGreaterThan(0);
  });

  test('no WASM ERROR log entries after calendar fetch with mocked API', async ({ page }) => {
    await stubBluetooth(page);
    await mockBackendApis(page);
    await mockIntervalsIcuApi(page);

    await page.addInitScript(({ prefix, athleteId, apiKey }) => {
      try {
        localStorage.setItem(`${prefix}/account/intervals_icu_api_key`,     apiKey);
        localStorage.setItem(`${prefix}/account/intervals_icu_athlete_id`,  athleteId);
      } catch (e) {
        console.warn('localStorage injection failed:', e);
      }
    }, { prefix: QSETTINGS_PREFIX, athleteId: TEST_ATHLETE_ID, apiKey: TEST_API_KEY });

    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
    await waitForAppReady(page);

    await page.waitForFunction(
      () => typeof window.mt_intervalsRefresh === 'function',
      null,
      { timeout: 30_000 },
    );
    await page.evaluate(() => window.mt_intervalsRefresh());
    await page.waitForTimeout(5_000);

    const errorLines = await getOverlayFatalErrorLines(page);
    expect(
      errorLines,
      `Unexpected WASM ERROR log entries after calendar fetch: ${errorLines.join('; ')}`,
    ).toHaveLength(0);
  });
});
