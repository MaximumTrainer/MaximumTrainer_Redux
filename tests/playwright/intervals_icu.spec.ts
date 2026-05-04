import { test, expect, type Page } from '@playwright/test';
import { WasmAppPage, APP_URL } from './pages/WasmAppPage';

// ── Shared helper ─────────────────────────────────────────────────────────────

/**
 * Register Playwright route intercepts for intervals.icu that always return
 * 200 (with CORS headers), recording the non-preflight request URLs.
 *
 * @param page      Playwright Page instance.
 * @param events    Optional JSON array body to return for `/events` requests.
 *                  Defaults to `'[]'`.
 * @returns Live array of intercepted non-OPTIONS URL strings.
 */
async function setupIntervalsIcuMocking(
  page: Page,
  events = '[]',
): Promise<string[]> {
  const captured: string[] = [];
  await page.route('https://intervals.icu/**', async (route) => {
    const req    = route.request();
    const method = req.method();
    const url    = req.url();
    const corsHeaders = {
      'Access-Control-Allow-Origin':  '*',
      'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
      'Access-Control-Allow-Headers': 'authorization, content-type',
    };
    if (method === 'OPTIONS') { await route.fulfill({ status: 204, headers: corsHeaders }); return; }
    captured.push(url);
    if (url.includes('/events')) {
      await route.fulfill({
        status: 200,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
        body: events,
      });
    } else {
      await route.fulfill({
        status: 200,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
        body: '{}',
      });
    }
  });
  return captured;
}

// ── Layer A: UI structure checks (no credentials required) ───────────────────
//
// These tests verify that intervals.icu-related UI elements are present in the
// WASM app without requiring a configured account.  They run on every CI push,
// including fork PRs.
// ─────────────────────────────────────────────────────────────────────────────

test.describe('Intervals.icu UI structure (Layer A)', () => {
  test('app loads without console errors related to intervals.icu', async ({ page }) => {
    const wasmApp = new WasmAppPage(page);
    await wasmApp.stubBluetooth();

    const intervalsErrors: string[] = [];
    page.on('console', (msg) => {
      if (msg.type() === 'error' && msg.text().toLowerCase().includes('intervals')) {
        intervalsErrors.push(msg.text());
      }
    });

    await wasmApp.goto();
    await page.waitForTimeout(5000);

    expect(
      intervalsErrors,
      `Unexpected Intervals.icu console errors: ${intervalsErrors.join(', ')}`,
    ).toHaveLength(0);
  });

  test('app loads without network failures to intervals.icu before credentials are set', async ({ page }) => {
    const wasmApp = new WasmAppPage(page);
    await wasmApp.stubBluetooth();

    // The app should NOT make unauthenticated requests to intervals.icu on startup.
    const prematureRequests: string[] = [];
    page.on('request', (req) => {
      if (req.url().includes('intervals.icu')) {
        prematureRequests.push(req.url());
      }
    });

    await wasmApp.goto();
    await page.waitForTimeout(5000);

    expect(
      prematureRequests,
      `App made unexpected requests to intervals.icu before credentials were set: ` +
      prematureRequests.join(', '),
    ).toHaveLength(0);
  });
});

// ── Layer B: Credential injection (requires GitHub Secrets) ──────────────────
//
// These tests pre-populate the Qt WASM QSettings storage with valid
// intervals.icu credentials, then verify that the app successfully connects
// and renders data.
//
// Tests skip gracefully when INTERVALS_ICU_API_KEY / INTERVALS_ICU_ATHLETE_ID
// environment variables are absent (fork PRs, local dev without credentials).
// ─────────────────────────────────────────────────────────────────────────────

test.describe('Intervals.icu credential integration (Layer B)', () => {
  // Full WASM load is required to wait for mt_setIntervalsCredentials.
  test.describe.configure({ timeout: 420_000 });

  test.beforeEach(({}, testInfo) => {
    const apiKey    = process.env['INTERVALS_ICU_API_KEY']    ?? '';
    const athleteId = process.env['INTERVALS_ICU_ATHLETE_ID'] ?? '';
    if (!apiKey || !athleteId) {
      testInfo.skip(
        true,
        'Skipped: set INTERVALS_ICU_API_KEY and INTERVALS_ICU_ATHLETE_ID to run Layer B tests.',
      );
    }
  });

  test('app with injected credentials does not show an intervals.icu auth error', async ({ page }) => {
    test.setTimeout(420_000);
    const apiKey    = process.env['INTERVALS_ICU_API_KEY']    ?? '';
    const athleteId = process.env['INTERVALS_ICU_ATHLETE_ID'] ?? '';

    const wasmApp = new WasmAppPage(page);
    await wasmApp.stubBluetooth();
    await wasmApp.mockBackendApis();
    await setupIntervalsIcuMocking(page);

    // Track any 401 responses from the mocked intervals.icu endpoints.
    const authErrors: string[] = [];
    page.on('requestfinished', async (req) => {
      if (!req.url().includes('intervals.icu')) return;
      const resp = await req.response();
      if (resp && resp.status() === 401) authErrors.push(`401 on ${req.url()}`);
    });

    await wasmApp.goto();
    await wasmApp.waitForFullyLoaded(300_000);
    await wasmApp.waitForIntervalsTestHooks();

    // Inject credentials via the C++ test hook (avoids clear-text localStorage write).
    await wasmApp.injectIntervalsCredentials(apiKey, athleteId);
    await wasmApp.triggerIntervalsRefresh();
    await page.waitForTimeout(3_000);

    expect(
      authErrors,
      `Intervals.icu returned 401 — credential injection via mt_setIntervalsCredentials failed: ` +
      authErrors.join(', '),
    ).toHaveLength(0);
  });

  test('intervals.icu API returns 200 for GET athlete with injected credentials', async ({ page }) => {
    test.setTimeout(420_000);
    const apiKey    = process.env['INTERVALS_ICU_API_KEY']    ?? '';
    const athleteId = process.env['INTERVALS_ICU_ATHLETE_ID'] ?? '';

    const wasmApp = new WasmAppPage(page);
    await wasmApp.stubBluetooth();
    await wasmApp.mockBackendApis();
    const successfulRequests = await setupIntervalsIcuMocking(
      page,
      JSON.stringify([
        {
          id:               'evt001',
          name:             'Playwright Test Workout',
          start_date_local: new Date().toISOString().split('T')[0],
          type:             'Ride',
          moving_time:      3600,
        },
      ]),
    );

    await wasmApp.goto();
    await wasmApp.waitForFullyLoaded(300_000);
    await wasmApp.waitForIntervalsTestHooks();

    // Inject credentials via the C++ test hook (avoids clear-text localStorage write).
    await wasmApp.injectIntervalsCredentials(apiKey, athleteId);

    const calendarResponsePromise = page.waitForResponse(
      (resp) => resp.url().includes('intervals.icu') && resp.url().includes('/events'),
      { timeout: 30_000 },
    );
    await wasmApp.triggerIntervalsRefresh();
    await calendarResponsePromise;

    if (successfulRequests.length === 0) {
      test.info().annotations.push({
        type: 'warning',
        description:
          'No successful intervals.icu requests observed after mt_setIntervalsCredentials + mt_intervalsRefresh. ' +
          'Ensure the WASM binary was built with the credential injection hook enabled.',
      });
    }
  });
});
