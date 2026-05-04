// @ts-check
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
 *   A2. Login test hooks are exposed after the WASM app loads.
 *
 * Layer B – real credentials login verification (requires GitHub Secrets):
 *   B0. Real credentials are validated by a direct Node.js HTTPS call to the
 *       intervals.icu API (bypasses CORS).  All subsequent Layer B tests are
 *       gated on this – the suite only passes when real login succeeds.
 *   B1. WASM uses injected credentials in its intervals.icu API requests.
 *   B2. WASM constructs the correct Authorization header from the injected
 *       credentials.
 *   B3. No WASM errors are reported after a successful authenticated API call.
 *
 * Authentication proof strategy
 * ──────────────────────────────────────────────────────────────────────────────
 * Browser WASM → intervals.icu requests cannot be allowed through in the test
 * environment (the page is served from localhost:8080; CORS headers from
 * intervals.icu do not cover that origin in CI).  Instead:
 *
 *   1. A Node.js Playwright APIRequestContext performs the REAL HTTP call to
 *      https://intervals.icu/api/v1/athlete/{id} with the secret credentials.
 *      This proves the secrets are valid.  Node has no CORS restrictions.
 *
 *   2. In the browser, all intervals.icu routes are intercepted.  The
 *      Authorization header from the first intercepted non-OPTIONS request is
 *      captured and compared against the expected header derived from the
 *      secrets — proving the WASM app uses the correct credentials.
 *
 *   3. Intercepted requests are fulfilled with realistic 200 responses so the
 *      WASM app can proceed through its post-login state machine.
 *
 * Skipping behaviour
 * ──────────────────────────────────────────────────────────────────────────────
 * Layer B tests skip gracefully when credentials are absent (fork PRs, local
 * development without credentials).  When secrets ARE set, EVERY Layer B test
 * must pass — the suite cannot succeed by skipping.
 */

const { test, expect } = require('@playwright/test');
const {
  stubBluetooth,
  waitForAppReady,
  mockBackendApis,
  getOverlayFatalErrorLines,
} = require('./wasm-test-helpers');

const BASE_ORIGIN = process.env.PLAYWRIGHT_BASE_URL
  || 'https://maximumtrainer.github.io/MaximumTrainer_Redux';
const APP_URL = `${BASE_ORIGIN}/app/`;

const SCREENSHOT_DIR = 'test-results/wasm-screenshots';

// ─────────────────────────────────────────────────────────────────────────────
// Layer A – pre-authentication state (no credentials required)
// ─────────────────────────────────────────────────────────────────────────────
test.describe('Login verification – Layer A: pre-authentication state', () => {
  test.describe.configure({ timeout: 420_000 });

  /** @type {import('@playwright/test').Page} */
  let page;
  /** @type {import('@playwright/test').BrowserContext} */
  let ctx;
  /** @type {string[]} */
  let earlyIntervalRequests = [];

  test.beforeAll(async ({ browser }) => {
    test.setTimeout(420_000);
    ctx  = await browser.newContext();
    page = await ctx.newPage();

    await stubBluetooth(page);
    await mockBackendApis(page);

    // Track any intervals.icu requests made before credentials are injected.
    await page.route('https://intervals.icu/**', async (route) => {
      earlyIntervalRequests.push(
        `${route.request().method()} ${route.request().url()}`,
      );
      await route.continue();
    });

    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
    await waitForAppReady(page, 300_000);
    // Allow a startup window for any background API calls to fire.
    await page.waitForTimeout(5_000);
  });

  test.afterAll(async () => { await ctx.close(); });

  test('A1 – app loads without premature intervals.icu authentication requests', async () => {
    const dataRequests = earlyIntervalRequests.filter(r => !r.startsWith('OPTIONS'));
    expect(
      dataRequests,
      `App issued unexpected intervals.icu requests before credentials were set: ` +
      dataRequests.join(', '),
    ).toHaveLength(0);
  });

  test('A2 – login test hooks are exposed after WASM app loads', async () => {
    const hooksExist = await page.evaluate(
      () => typeof window.mt_setIntervalsCredentials === 'function' &&
            typeof window.mt_intervalsRefresh === 'function',
    );
    expect(
      hooksExist,
      'window.mt_setIntervalsCredentials and window.mt_intervalsRefresh ' +
      'must both be functions after the WASM app loads.',
    ).toBe(true);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Layer B – real credentials login verification
// ─────────────────────────────────────────────────────────────────────────────
test.describe('Login verification – Layer B: real credentials', () => {
  test.describe.configure({ timeout: 420_000 });

  const apiKey    = process.env.INTERVALS_ICU_API_KEY    || '';
  const athleteId = process.env.INTERVALS_ICU_ATHLETE_ID || '';
  const hasCredentials = !!(apiKey && athleteId);

  // ── Shared state populated in beforeAll ────────────────────────────────────
  /** @type {import('@playwright/test').Page} */
  let page;
  /** @type {import('@playwright/test').BrowserContext} */
  let ctx;

  // Real HTTP response from the Node.js credential validation call.
  let realApiStatus = 0;
  let realApiAthleteId = '';

  // Captured from browser-side route interception.
  /** @type {{ method: string; url: string; auth: string }[]} */
  let capturedRequests = [];

  test.beforeAll(async ({ browser, playwright }) => {
    if (!hasCredentials) return; // Nothing to set up — each test skips individually.

    test.setTimeout(420_000);

    // ── Step 1: Validate credentials with a real Node.js HTTP call ────────────
    // Node has no CORS restrictions; this proves the secrets are valid before
    // we test the WASM browser path.
    const apiContext = await playwright.request.newContext({
      baseURL: 'https://intervals.icu',
      extraHTTPHeaders: {
        Authorization: 'Basic ' + Buffer.from(`API_KEY:${apiKey}`).toString('base64'),
        Accept: 'application/json',
      },
    });

    try {
      const resp = await apiContext.get(`/api/v1/athlete/${athleteId}`);
      realApiStatus = resp.status();

      if (realApiStatus === 200) {
        const body = await resp.json();
        realApiAthleteId = String(body.id || body.athlete_id || '');
      }
    } finally {
      await apiContext.dispose();
    }

    // ── Step 2: Launch WASM browser with route interception ───────────────────
    ctx  = await browser.newContext();
    page = await ctx.newPage();

    await stubBluetooth(page);
    await mockBackendApis(page);

    // Intercept intervals.icu requests to:
    //   a) capture the Authorization header emitted by the WASM app, and
    //   b) fulfill with mock 200 responses (CORS-safe in localhost test env).
    await page.route('https://intervals.icu/**', async (route) => {
      const req    = route.request();
      const method = req.method();
      const url    = req.url();
      const auth   = req.headers()['authorization'] || '';

      capturedRequests.push({ method, url, auth });

      const corsHeaders = {
        'Access-Control-Allow-Origin':  '*',
        'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
        // Reflect the requested headers to handle any new headers Qt might add.
        'Access-Control-Allow-Headers':
          req.headers()['access-control-request-headers'] || 'authorization, content-type',
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
              id: 'evt001',
              name: 'Login Verification Test Workout',
              start_date_local: new Date().toISOString().split('T')[0],
              type: 'Ride',
              moving_time: 3600,
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

    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
    await waitForAppReady(page, 300_000);

    // Wait for both test hooks to be registered by the WASM C++ code.
    await page.waitForFunction(
      () => typeof window.mt_setIntervalsCredentials === 'function' &&
            typeof window.mt_intervalsRefresh === 'function',
      null,
      { timeout: 120_000 },
    );

    // ── Step 3: Inject real credentials into the running WASM app ─────────────
    await page.evaluate(
      ({ key, id }) => window.mt_setIntervalsCredentials(key, id),
      { key: apiKey, id: athleteId },
    );

    // Listen for the /events response before triggering refresh (avoids race).
    const calendarResponsePromise = page.waitForResponse(
      resp => resp.url().includes('intervals.icu') && resp.url().includes('/events'),
      { timeout: 30_000 },
    );

    await page.evaluate(() => window.mt_intervalsRefresh());
    await calendarResponsePromise;

    // Capture a screenshot as login evidence.
    const ts = new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
    await require('fs').promises.mkdir(SCREENSHOT_DIR, { recursive: true }).catch(() => {});
    await page.screenshot({
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
      `Expected 200 — check that INTERVALS_ICU_API_KEY and INTERVALS_ICU_ATHLETE_ID ` +
      `are correct and the account has API access enabled.`,
    ).toBe(200);
  });

  // ── B1: WASM makes API request after credentials are injected ────────────────
  test('B1 – WASM issues an intervals.icu API request after credentials are injected', async () => {
    if (!hasCredentials) {
      test.skip(
        true,
        'Skipped: set INTERVALS_ICU_API_KEY and INTERVALS_ICU_ATHLETE_ID ' +
        'to run Intervals.icu login verification tests.',
      );
    }

    const dataRequests = capturedRequests.filter(r => r.method !== 'OPTIONS');
    expect(
      dataRequests.length,
      `No intervals.icu data requests were made after mt_setIntervalsCredentials() + ` +
      `mt_intervalsRefresh().  The credential injection or refresh pipeline may be broken.`,
    ).toBeGreaterThan(0);
  });

  // ── B2: WASM uses the correct Authorization header ───────────────────────────
  test('B2 – WASM constructs the correct Authorization header from injected credentials', async () => {
    if (!hasCredentials) {
      test.skip(
        true,
        'Skipped: set INTERVALS_ICU_API_KEY and INTERVALS_ICU_ATHLETE_ID ' +
        'to run Intervals.icu login verification tests.',
      );
    }

    // Find the first non-OPTIONS request that carries an Authorization header.
    const authRequest = capturedRequests.find(
      r => r.method !== 'OPTIONS' && r.auth !== '',
    );

    expect(
      authRequest,
      'No intercepted intervals.icu request contained an Authorization header. ' +
      'The WASM app may not be attaching credentials to its API calls.',
    ).toBeTruthy();

    // Qt encodes credentials as: "API_KEY:" + apiKey → UTF-8 bytes → Base64
    // This must match `Buffer.from("API_KEY:" + apiKey).toString("base64")`.
    const expectedBase64 = Buffer.from(`API_KEY:${apiKey}`).toString('base64');
    const expectedHeader  = `Basic ${expectedBase64}`;

    expect(
      authRequest.auth,
      `Authorization header mismatch.\n` +
      `Expected: "${expectedHeader}"\n` +
      `Received: "${authRequest.auth}"\n` +
      `This indicates the injected credentials were not used correctly in the WASM app's API call.`,
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

    const errorLines = await getOverlayFatalErrorLines(page);
    expect(
      errorLines,
      `WASM reported errors after authenticated login:\n${errorLines.join('\n')}`,
    ).toHaveLength(0);
  });
});
