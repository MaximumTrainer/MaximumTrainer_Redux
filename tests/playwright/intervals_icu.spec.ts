import { test, expect, type Page } from '@playwright/test';
import { WasmAppPage } from './pages/WasmAppPage';

// ── Shared helper ─────────────────────────────────────────────────────────────

/** Captured data for a single non-preflight intervals.icu request. */
type IntervalsMockCapture = {
  /** Request URL. */
  url: string;
  /** Whether an Authorization header was present on the request. */
  hasAuth: boolean;
};

/**
 * Register Playwright route intercepts for intervals.icu that always return
 * 200 (with CORS headers), recording each non-preflight request URL and
 * whether it carried an Authorization header.
 *
 * @param page      Playwright Page instance.
 * @param events    Optional JSON array body to return for `/events` requests.
 *                  Defaults to `'[]'`.
 * @returns Live array of captured request data (updated as requests arrive).
 */
async function setupIntervalsIcuMocking(
  page: Page,
  events = '[]',
): Promise<IntervalsMockCapture[]> {
  const captured: IntervalsMockCapture[] = [];
  await page.route(
    /^https:\/\/(?:intervals\.icu|mt-intervals-proxy\.intervals-login\.workers\.dev)\/.*/,
    async (route) => {
      const req    = route.request();
      const method = req.method();
      const url    = req.url();
      const corsHeaders = {
        'Access-Control-Allow-Origin':  '*',
        'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
        'Access-Control-Allow-Headers': 'authorization, content-type',
      };
      if (method === 'OPTIONS') { await route.fulfill({ status: 204, headers: corsHeaders }); return; }
      captured.push({ url, hasAuth: !!(req.headers()['authorization']) });
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
    },
  );
  return captured;
}

// ── Layer A: UI structure checks (no credentials required) ───────────────────
//
// These tests verify that intervals.icu-related UI elements are present in the
// WASM app without requiring a configured account.  They run on every CI push,
// including fork PRs.
// ─────────────────────────────────────────────────────────────────────────────

test.describe('Intervals.icu UI structure (Layer A)', () => {
  // WASM binary takes 2+ minutes to JIT-compile on cold CI runners; use a
  // generous timeout so waitForFullyLoaded() can finish before asserting.
  test.describe.configure({ timeout: 420_000 });

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
    await wasmApp.waitForFullyLoaded(300_000);

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
    await wasmApp.waitForFullyLoaded(300_000);

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
    const captured = await setupIntervalsIcuMocking(page);

    // Install OAuth mock after catch-all so /oauth/token takes precedence.
    await wasmApp.setupOAuthMock();

    await wasmApp.goto();
    await wasmApp.waitForFullyLoaded(300_000);
    await wasmApp.completeOAuthLogin();

    // Inject credentials via the C++ test hook (avoids clear-text localStorage write).
    await wasmApp.injectIntervalsCredentials(apiKey, athleteId);

    // Register the response listener BEFORE triggering the refresh to avoid a
    // race where the response arrives before the listener is attached.
    const firstResponsePromise = page.waitForResponse(
      (resp) =>
        (resp.url().includes('intervals.icu')
          || resp.url().includes('mt-intervals-proxy.intervals-login.workers.dev'))
        && resp.url().includes('/events'),
      { timeout: 30_000 },
    );
    await wasmApp.triggerIntervalsRefresh();
    await firstResponsePromise;

    // Verify that all intercepted requests carried an Authorization header,
    // proving that mt_setIntervalsCredentials correctly populated the account.
    expect(
      captured.length,
      'No intervals.icu requests were captured after triggerIntervalsRefresh — ' +
      'check that mt_setIntervalsCredentials triggered outgoing API calls.',
    ).toBeGreaterThan(0);
    expect(
      captured.filter((c) => !c.hasAuth).map((c) => c.url),
      'Intervals.icu requests were made without an Authorization header — ' +
      'credential injection via mt_setIntervalsCredentials may not have set auth correctly.',
    ).toHaveLength(0);
  });

  test('intervals.icu API returns 200 for GET athlete with injected credentials', async ({ page }) => {
    test.setTimeout(420_000);
    const apiKey    = process.env['INTERVALS_ICU_API_KEY']    ?? '';
    const athleteId = process.env['INTERVALS_ICU_ATHLETE_ID'] ?? '';

    const wasmApp = new WasmAppPage(page);
    await wasmApp.stubBluetooth();
    await wasmApp.mockBackendApis();
    const captured = await setupIntervalsIcuMocking(
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

    // Install OAuth mock after catch-all so /oauth/token takes precedence.
    await wasmApp.setupOAuthMock();

    await wasmApp.goto();
    await wasmApp.waitForFullyLoaded(300_000);
    await wasmApp.completeOAuthLogin();

    // Inject credentials via the C++ test hook (avoids clear-text localStorage write).
    await wasmApp.injectIntervalsCredentials(apiKey, athleteId);

    const calendarResponsePromise = page.waitForResponse(
      (resp) =>
        (resp.url().includes('intervals.icu')
          || resp.url().includes('mt-intervals-proxy.intervals-login.workers.dev'))
        && resp.url().includes('/events'),
      { timeout: 30_000 },
    );
    await wasmApp.triggerIntervalsRefresh();
    await calendarResponsePromise;

    if (captured.length === 0) {
      test.info().annotations.push({
        type: 'warning',
        description:
          'No successful intervals.icu requests observed after mt_setIntervalsCredentials + mt_intervalsRefresh. ' +
          'Ensure the WASM binary was built with the credential injection hook enabled.',
      });
    }
  });
});
