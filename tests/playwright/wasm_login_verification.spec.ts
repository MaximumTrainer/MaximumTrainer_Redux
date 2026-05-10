/**
 * Intervals.icu Login Verification – WASM
 *
 * Verifies that MaximumTrainer WASM correctly authenticates with Intervals.icu
 * using real credentials supplied via GitHub Actions secrets.
 *
 * Test structure
 * ──────────────────────────────────────────────────────────────────────────────
 * Layer A – pre-authentication state (no credentials required, runs always):
 *   A1. App loads without premature intervals.icu API calls.
 *   A2. The mt_loginWithApiKey test hook is exposed after the WASM app loads,
 *       allowing Playwright to trigger the API-key login form programmatically.
 *
 * Layer B – real credentials login verification (requires GitHub Secrets):
 *   B0. Real credentials are validated by a direct Node.js HTTPS call to the
 *       intervals.icu API (bypasses CORS).  All subsequent Layer B tests are
 *       gated on this – the suite only passes when real login succeeds.
 *   B1. WASM login form accepts credentials and reaches MainWindow (verified
 *       by mt_setIntervalsCredentials and mt_intervalsRefresh hooks appearing).
 *   B2. WASM uses the correct Authorization header in post-login API requests.
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
    await wasmApp.page.route('https://intervals.icu/**', async (route) => {
      earlyIntervalRequests.push(
        `${route.request().method()} ${route.request().url()}`,
      );
      await route.continue();
    });

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

  test('A2 – login test hook (mt_loginWithApiKey) is exposed after WASM app loads', async () => {
    const hookExists = await wasmApp.page.evaluate(
      () => typeof (window as any).mt_loginWithApiKey === 'function',
    );
    expect(
      hookExists,
      'window.mt_loginWithApiKey must be a function after the WASM app loads. ' +
      'This hook allows Playwright to submit the API-key login form programmatically.',
    ).toBe(true);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Layer B – real credentials login verification
// ─────────────────────────────────────────────────────────────────────────────
test.describe('Login verification – Layer B: real credentials', () => {
  test.describe.configure({ timeout: 420_000 });

  const apiKey      = process.env['INTERVALS_ICU_API_KEY']    ?? '';
  const athleteId   = process.env['INTERVALS_ICU_ATHLETE_ID'] ?? '';
  const hasCredentials = !!(apiKey && athleteId);

  let wasmApp: WasmAppPage;
  let ctx: import('@playwright/test').BrowserContext;

  // Real HTTP response from the Node.js credential validation call.
  let realApiStatus    = 0;
  let realApiAthleteId = '';

  // Captured from browser-side route interception.
  let capturedRequests: Array<{ method: string; url: string; auth: string }> = [];

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
      if (realApiStatus === 200) {
        const body = await resp.json() as Record<string, unknown>;
        realApiAthleteId = String(body['id'] ?? body['athlete_id'] ?? '');
      }
    } finally {
      await apiContext.dispose();
    }

    // ── Step 2: Launch WASM browser with route interception ───────────────────
    ctx = await browser.newContext();
    wasmApp = new WasmAppPage(await ctx.newPage());

    await wasmApp.stubBluetooth();
    await wasmApp.mockBackendApis();

    // Intercept intervals.icu to capture the Authorization header and fulfil
    // with mock 200 responses (CORS-safe in the localhost test environment).
    await wasmApp.page.route('https://intervals.icu/**', async (route) => {
      const req    = route.request();
      const method = req.method();
      const url    = req.url();
      const auth   = req.headers()['authorization'] ?? '';

      capturedRequests.push({ method, url, auth });

      const corsHeaders = {
        'Access-Control-Allow-Origin':  '*',
        'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
        'Access-Control-Allow-Headers':
          req.headers()['access-control-request-headers'] ?? 'authorization, content-type',
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
          body: JSON.stringify({ id: athleteId }),
        });
      }
    });

    await wasmApp.goto();
    await wasmApp.waitForFullyLoaded(300_000);

    // ── Step 3: Wait for the login hook and submit the API-key login form ─────
    // The WASM app now shows a login form instead of going offline automatically.
    // mt_loginWithApiKey fills and submits the form, which causes DialogLogin to
    // validate credentials (via the mocked intervals.icu route above) and then
    // create MainWindow on success.
    await wasmApp.page.waitForFunction(
      () => typeof (window as any).mt_loginWithApiKey === 'function',
      null,
      { timeout: 60_000 },
    );

    await wasmApp.page.evaluate(
      ({ key, id }: { key: string; id: string }) =>
        (window as any).mt_loginWithApiKey(id, key),
      { key: apiKey, id: athleteId },
    );

    // ── Step 4: Wait for MainWindow to appear (login succeeded) ──────────────
    // After successful login, TabIntervalsIcu registers mt_setIntervalsCredentials
    // and mt_intervalsRefresh.  Their presence confirms MainWindow is ready.
    await wasmApp.waitForIntervalsTestHooks(120_000);

    // ── Step 5: Trigger a calendar refresh and wait for an /events response ───
    const calendarResponsePromise = wasmApp.page.waitForResponse(
      (resp) =>
        resp.url().includes('intervals.icu') && resp.url().includes('/events'),
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
    expect(
      realApiStatus,
      `Intervals.icu API returned HTTP ${realApiStatus} for athlete ${athleteId}. ` +
      `Expected 200 — verify that INTERVALS_ICU_API_KEY and INTERVALS_ICU_ATHLETE_ID ` +
      `are correctly set as GitHub Actions secrets / environment variables.`,
    ).toBe(200);
  });

  // ── B1: WASM login form accepts credentials and MainWindow loads ─────────────
  test('B1 – WASM login form accepts credentials and MainWindow loads', async () => {
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
      'No intervals.icu data requests were made after API-key login.',
    ).toBeGreaterThan(0);
  });

  // ── B2: WASM uses the correct Authorization header ───────────────────────────
  test('B2 – WASM constructs the correct Authorization header from API key credentials', async () => {
    if (!hasCredentials) {
      test.skip(
        true,
        'Skipped: set INTERVALS_ICU_API_KEY and INTERVALS_ICU_ATHLETE_ID ' +
        'to run Intervals.icu login verification tests.',
      );
    }

    const authRequest = capturedRequests.find(
      (r) => r.method !== 'OPTIONS' && r.auth !== '',
    );
    expect(
      authRequest,
      'No intercepted intervals.icu request contained an Authorization header.',
    ).toBeTruthy();

    const expectedBase64 = Buffer.from(`API_KEY:${apiKey}`).toString('base64');
    const expectedHeader  = `Basic ${expectedBase64}`;

    expect(
      authRequest!.auth,
      `Authorization header mismatch.\nExpected: "${expectedHeader}"\nReceived: "${authRequest!.auth}"`,
    ).toBe(expectedHeader);
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
