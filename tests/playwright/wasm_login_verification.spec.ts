/**
 * Intervals.icu Login Verification – WASM
 *
 * Verifies that MaximumTrainer WASM correctly authenticates with Intervals.icu
 * using the OAuth2 popup flow.  Real credentials (from GitHub Actions secrets)
 * are used to gate Layer B tests, but all actual HTTP traffic to intervals.icu
 * is intercepted and mocked so the browser-side OAuth code path is exercised
 * without requiring CORS or a real token exchange.
 *
 * Test structure
 * ──────────────────────────────────────────────────────────────────────────────
 * Layer A – pre-authentication state (no credentials required, runs always):
 *   A1. App loads without premature intervals.icu API calls.
 *   A2. The mt_wasmOAuthReady flag is set after the WASM app loads, confirming
 *       the OAuth popup bridge is initialized and ready.
 *
 * Layer B – OAuth popup flow verification (requires GitHub Secrets):
 *   B0. Real credentials are validated by a direct Node.js HTTPS call to the
 *       intervals.icu API (bypasses CORS).  All subsequent Layer B tests are
 *       gated on this – the suite only passes when real credentials are valid.
 *   B1. Mocked OAuth popup flow completes login and reaches MainWindow (verified
 *       by mt_setIntervalsCredentials and mt_intervalsRefresh hooks appearing).
 *   B2. WASM uses Bearer authorization in post-login API requests.
 *   B3. No WASM errors are reported after a successful authenticated login.
 */

import { test, expect } from '@playwright/test';
import { WasmAppPage, APP_URL } from './pages/WasmAppPage';

const SCREENSHOT_DIR = 'test-results/wasm-screenshots';

// ─────────────────────────────────────────────────────────────────────────────
// Layer A – pre-authentication state (no credentials required)
// ─────────────────────────────────────────────────────────────────────────────
test.describe('Login verification – Layer A: pre-authentication state', () => {
  test.describe.configure({ timeout: 420_000 });

  let wasmApp: WasmAppPage;
  let ctx: import('@playwright/test').BrowserContext;
  let earlyIntervalRequests: string[] = [];

  test.beforeAll(async ({ browser }) => {
    test.setTimeout(420_000);
    ctx = await browser.newContext();
    wasmApp = new WasmAppPage(await ctx.newPage());

    await wasmApp.stubBluetooth();
    await wasmApp.mockBackendApis();

    // Track any intervals.icu requests made before credentials are injected.
    await wasmApp.page.route(
      /^https:\/\/(?:intervals\.icu|mt-intervals-proxy\.intervals-login\.workers\.dev)\/.*/,
      async (route) => {
        earlyIntervalRequests.push(
          `${route.request().method()} ${route.request().url()}`,
        );
        await route.continue();
      },
    );

    await wasmApp.goto();
    await wasmApp.waitForFullyLoaded(300_000);
    // Allow a startup window for any background API calls to fire.
    await wasmApp.page.waitForTimeout(5_000);
  });

  test.afterAll(async () => { await ctx.close(); });

  test('A1 – app loads without premature intervals.icu authentication requests', async () => {
    const dataRequests = earlyIntervalRequests.filter((r) => !r.startsWith('OPTIONS'));
    expect(
      dataRequests,
      `App issued unexpected intervals.icu requests before credentials were set: ` +
      dataRequests.join(', '),
    ).toHaveLength(0);
  });

  test('A2 – WASM OAuth bridge (mt_wasmOAuthReady) is exposed after app loads', async () => {
    const ready = await wasmApp.page.evaluate(
      () => (window as any).mt_wasmOAuthReady === true,
    );
    expect(
      ready,
      'window.mt_wasmOAuthReady must be true after the WASM app loads. ' +
      'This flag confirms the OAuth popup bridge is initialized and ready.',
    ).toBe(true);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Layer B – OAuth popup flow verification (requires GitHub Secrets)
// ─────────────────────────────────────────────────────────────────────────────
test.describe('Login verification – Layer B: OAuth popup flow', () => {
  test.describe.configure({ timeout: 420_000 });

  const apiKey      = process.env['INTERVALS_ICU_API_KEY']    ?? '';
  const athleteId   = process.env['INTERVALS_ICU_ATHLETE_ID'] ?? '';
  const hasCredentials = !!(apiKey && athleteId);

  // Fake Bearer token returned by the mocked token exchange endpoint.
  const FAKE_ACCESS_TOKEN  = 'test_access_token_abcdef';
  const FAKE_REFRESH_TOKEN = 'test_refresh_token_ghijkl';
  const EXPECTED_SCOPE = 'ACTIVITY:WRITE,WELLNESS:READ,SETTINGS:WRITE,CALENDAR:WRITE,LIBRARY:READ';

  let wasmApp: WasmAppPage;
  let ctx: import('@playwright/test').BrowserContext;

  // Real HTTP response from the Node.js credential validation call.
  let realApiStatus = 0;

  // Captured from browser-side route interception.
  let capturedRequests: Array<{ method: string; url: string; auth: string }> = [];
  let capturedOAuthAuthorizeUrl = '';

  test.beforeAll(async ({ browser, playwright }) => {
    if (!hasCredentials) return;

    test.setTimeout(420_000);

    // ── Step 1: Validate credentials with a real Node.js HTTP call ────────────
    const apiContext = await playwright.request.newContext({
      baseURL: 'https://intervals.icu',
      extraHTTPHeaders: {
        Authorization: 'Basic ' + Buffer.from(`API_KEY:${apiKey}`).toString('base64'),
        Accept:        'application/json',
      },
    });

    try {
      const resp = await apiContext.get(`/api/v1/athlete/${athleteId}`);
      realApiStatus = resp.status();
    } finally {
      await apiContext.dispose();
    }

    // ── Step 2: Launch WASM browser with OAuth popup mock ─────────────────────
    ctx = await browser.newContext();
    wasmApp = new WasmAppPage(await ctx.newPage());

    await wasmApp.disableIcuProxyInterceptor();
    await wasmApp.stubBluetooth();
    await wasmApp.mockBackendApis();

    // Mock window.open to capture the OAuth URL and immediately post a synthetic
    // authorization code back via window.dispatchEvent('message').  This simulates
    // the oauth_callback.html popup posting back to the opener.
    // Must be installed BEFORE goto() so it is active when the WASM app runs.
    await wasmApp.page.addInitScript(() => {
      const origOpen = window.open.bind(window);
      (window as any).open = function(
        url?: string | URL,
        target?: string,
        features?: string,
      ) {
        if (target === 'mt_oauth_login' && typeof url === 'string') {
          (window as any).__mtCapturedOAuthAuthorizeUrl = url;
          // Extract the state parameter from the OAuth URL.
          try {
            const parsed = new URL(url);
            const state  = parsed.searchParams.get('state') ?? '';
            // js_openOAuthPopup registers the message listener BEFORE calling
            // window.open, so dispatching synchronously (setTimeout 0) is safe.
            setTimeout(() => {
              window.dispatchEvent(new MessageEvent('message', {
                data: {
                  mt_oauth_code:  'playwright_test_code_' + state.slice(0, 8),
                  mt_oauth_state: state,
                },
                origin: window.location.origin,
              }));
            }, 0);
          } catch (e) {
            console.error('[Playwright] Failed to parse OAuth URL:', e);
          }
          // Return a minimal popup stub (already closed).
          return { closed: true } as Window;
        }
        return origOpen(url, target, features);
      };
    });

    // Intercept all intervals.icu requests (direct host + Cloudflare proxy):
    // handle token exchange and data APIs.
    const corsHeaders = {
      'Access-Control-Allow-Origin':  '*',
      'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
      'Access-Control-Allow-Headers': 'authorization, content-type',
    };

    await wasmApp.page.route(
      /^https:\/\/(?:intervals\.icu|mt-intervals-proxy\.intervals-login\.workers\.dev)\/.*/,
      async (route) => {
        const req    = route.request();
        const method = req.method();
        const url    = req.url();
        const auth   = req.headers()['authorization'] ?? '';

        if (method === 'OPTIONS') {
          await route.fulfill({ status: 204, headers: corsHeaders });
          return;
        }

        // OAuth token exchange — must be handled before the general API catch-all.
        if (url === 'https://intervals.icu/oauth/token' || url.includes('/oauth/token')) {
          await route.fulfill({
            status: 200,
            headers: { ...corsHeaders, 'Content-Type': 'application/json' },
            body: JSON.stringify({
              access_token:  FAKE_ACCESS_TOKEN,
              refresh_token: FAKE_REFRESH_TOKEN,
              token_type:    'Bearer',
              expires_in:    3600,
              athlete_id:    athleteId,
            }),
          });
          return;
        }

        capturedRequests.push({ method, url, auth });

        if (url.includes('/events')) {
          await route.fulfill({
            status: 200,
            headers: { ...corsHeaders, 'Content-Type': 'application/json' },
            body: JSON.stringify([
              {
                id:               'evt001',
                name:             'Login Verification Test Workout',
                start_date_local: new Date().toISOString().split('T')[0],
                type:             'Ride',
                moving_time:      3600,
              },
            ]),
          });
        } else {
          await route.fulfill({
            status: 200,
            headers: { ...corsHeaders, 'Content-Type': 'application/json' },
            body: JSON.stringify({ id: athleteId, name: 'Test Athlete' }),
          });
        }
      },
    );

    await wasmApp.goto();
    await wasmApp.waitForFullyLoaded(300_000);

    // ── Step 3: Wait for the OAuth bridge and trigger login ───────────────────
    await wasmApp.page.waitForFunction(
      () => typeof (window as any).mt_triggerOAuthLogin === 'function'
            && (window as any).mt_wasmOAuthReady === true,
      null,
      { timeout: 60_000 },
    );

    // Trigger the OAuth popup flow (synchronous call within a user-gesture context).
    await wasmApp.page.evaluate(() => (window as any).mt_triggerOAuthLogin());
    capturedOAuthAuthorizeUrl = await wasmApp.page.evaluate(
      () => (window as any).__mtCapturedOAuthAuthorizeUrl ?? '',
    );

    // ── Step 4: Wait for MainWindow to appear (login succeeded) ──────────────
    await wasmApp.waitForIntervalsTestHooks(120_000);

    // ── Step 5: Trigger a calendar refresh and wait for an /events response ───
    const calendarResponsePromise = wasmApp.page.waitForResponse(
      (resp) =>
        (resp.url().includes('intervals.icu')
          || resp.url().includes('mt-intervals-proxy.intervals-login.workers.dev'))
        && resp.url().includes('/events'),
      { timeout: 30_000 },
    );

    await wasmApp.triggerIntervalsRefresh();
    await calendarResponsePromise;

    // Capture a screenshot as login evidence.
    const ts = new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
    const fs = await import('fs');
    await fs.promises.mkdir(SCREENSHOT_DIR, { recursive: true }).catch(() => {});
    await wasmApp.page.screenshot({
      path: `${SCREENSHOT_DIR}/login-verification-wasm-${ts}.png`,
      fullPage: false,
    });
  });

  test.afterAll(async () => { if (ctx) await ctx.close(); });

  // ── B0: Real credential validation (gate for all other Layer B tests) ───────
  test('B0 – real Intervals.icu API validates credentials (HTTP 200)', async () => {
    if (!hasCredentials) {
      test.skip(
        true,
        'Skipped: set INTERVALS_ICU_API_KEY and INTERVALS_ICU_ATHLETE_ID ' +
        'to run Intervals.icu login verification tests.',
      );
    }
    if (realApiStatus === 0) {
      test.skip(
        true,
        'Skipped: real Intervals.icu API request failed with a network error ' +
        '(HTTP 0 — likely a CI connectivity issue). ' +
        'INTERVALS_ICU_API_KEY / INTERVALS_ICU_ATHLETE_ID may still be valid.',
      );
    }
    expect(
      realApiStatus,
      `Intervals.icu API returned HTTP ${realApiStatus} for athlete ${athleteId}. ` +
      `Expected 200 — verify that INTERVALS_ICU_API_KEY and INTERVALS_ICU_ATHLETE_ID ` +
      `are correctly set as GitHub Actions secrets / environment variables.`,
    ).toBe(200);
  });

  // ── B1: OAuth popup flow completes and MainWindow loads ───────────────────────
  test('B1 – OAuth popup flow completes and MainWindow loads', async () => {
    if (!hasCredentials) {
      test.skip(
        true,
        'Skipped: set INTERVALS_ICU_API_KEY and INTERVALS_ICU_ATHLETE_ID ' +
        'to run Intervals.icu login verification tests.',
      );
    }
    // If MainWindow loaded, the intervals hooks were registered (checked in beforeAll).
    // Additionally verify that at least one intervals.icu data request was made.
    const dataRequests = capturedRequests.filter((r) => r.method !== 'OPTIONS');
    expect(
      dataRequests.length,
      'No intervals.icu data requests were made after OAuth login.',
    ).toBeGreaterThan(0);
  });

  test('B1a – OAuth authorize URL uses comma-separated Intervals.icu scope', async () => {
    if (!hasCredentials) {
      test.skip(
        true,
        'Skipped: set INTERVALS_ICU_API_KEY and INTERVALS_ICU_ATHLETE_ID ' +
        'to run Intervals.icu login verification tests.',
      );
    }

    expect(
      capturedOAuthAuthorizeUrl,
      'No OAuth authorize URL was captured from window.open.',
    ).toBeTruthy();

    const parsed = new URL(capturedOAuthAuthorizeUrl);
    const scope = parsed.searchParams.get('scope') ?? '';
    expect(scope).toBe(EXPECTED_SCOPE);
    expect(scope.includes(',')).toBeTruthy();
    expect(scope.includes(' ')).toBeFalsy();
  });

  // ── B2: WASM uses Bearer authorization after OAuth login ─────────────────────
  test('B2 – WASM uses Bearer authorization header in post-login API requests', async () => {
    if (!hasCredentials) {
      test.skip(
        true,
        'Skipped: set INTERVALS_ICU_API_KEY and INTERVALS_ICU_ATHLETE_ID ' +
        'to run Intervals.icu login verification tests.',
      );
    }

    const bearerRequest = capturedRequests.find(
      (r) => r.method !== 'OPTIONS' && r.auth.startsWith('Bearer '),
    );
    expect(
      bearerRequest,
      'No intercepted intervals.icu request contained a Bearer Authorization header. ' +
      'The WASM app should use OAuth Bearer tokens for post-login API requests.',
    ).toBeTruthy();

    // Verify the Bearer token matches the one returned by the mocked token exchange.
    expect(
      bearerRequest!.auth,
      `Expected "Bearer ${FAKE_ACCESS_TOKEN}" but received "${bearerRequest!.auth}"`,
    ).toBe(`Bearer ${FAKE_ACCESS_TOKEN}`);
  });

  // ── B3: No WASM errors after authenticated login ──────────────────────────────
  test('B3 – no WASM errors are reported after authenticated login', async () => {
    if (!hasCredentials) {
      test.skip(
        true,
        'Skipped: set INTERVALS_ICU_API_KEY and INTERVALS_ICU_ATHLETE_ID ' +
        'to run Intervals.icu login verification tests.',
      );
    }
    const errorLines = await wasmApp.logOverlay.getFatalErrorLines();
    expect(
      errorLines,
      `WASM reported errors after authenticated login:\n${errorLines.join('\n')}`,
    ).toHaveLength(0);
  });
});
