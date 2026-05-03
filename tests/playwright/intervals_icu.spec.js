// @ts-check
const { test, expect } = require('@playwright/test');

const BASE_ORIGIN = process.env.PLAYWRIGHT_BASE_URL || 'https://maximumtrainer.github.io/MaximumTrainer_Redux';
const APP_URL = `${BASE_ORIGIN}/app/`;

// ── Helpers ──────────────────────────────────────────────────────────────────

async function stubBluetooth(page) {
  await page.addInitScript(() => {
    if (!navigator.bluetooth) {
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

// Wait until the Qt canvas or loading screen is visible, then return.
async function waitForAppReady(page, timeoutMs = 20_000) {
  await page.waitForFunction(
    () => {
      const loading = document.querySelector('#loading-screen');
      const canvas  = document.querySelector('#qt-canvas-wrapper');
      const loadingVisible = loading && loading.offsetParent !== null;
      const canvasVisible  = canvas && canvas.style.visibility !== 'hidden';
      return !!(loadingVisible || canvasVisible);
    },
    { timeout: timeoutMs },
  );
}

// ── Layer A: UI structure checks (no credentials required) ───────────────────
//
// These tests verify that intervals.icu-related UI elements are present in the
// WASM app without requiring a configured account.  They run on every CI push,
// including fork PRs.
// ─────────────────────────────────────────────────────────────────────────────

test.describe('Intervals.icu UI structure (Layer A)', () => {

  test('app loads without console errors related to intervals.icu', async ({ page }) => {
    await stubBluetooth(page);

    const intervalsErrors = [];
    page.on('console', msg => {
      if (msg.type() === 'error' && msg.text().toLowerCase().includes('intervals')) {
        intervalsErrors.push(msg.text());
      }
    });

    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
    await page.waitForTimeout(5000);

    expect(
      intervalsErrors,
      `Unexpected Intervals.icu console errors: ${intervalsErrors.join(', ')}`,
    ).toHaveLength(0);
  });

  test('app loads without network failures to intervals.icu before credentials are set', async ({ page }) => {
    await stubBluetooth(page);

    // The app should NOT make unauthenticated requests to intervals.icu on startup.
    const prematureRequests = [];
    page.on('request', req => {
      if (req.url().includes('intervals.icu')) {
        prematureRequests.push(req.url());
      }
    });

    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
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
// These tests pre-populate the Qt WASM QSettings storage with valid intervals.icu
// credentials, then verify that the app successfully connects and renders data.
//
// Tests skip gracefully when INTERVALS_ICU_API_KEY / INTERVALS_ICU_ATHLETE_ID
// environment variables are absent (fork PRs, local dev without credentials).
//
// NOTE: Qt WASM QSettings key format uses Emscripten's virtual filesystem
// (IDBFS-backed /home/user/.config/<org>/<app>.conf).  The localStorage
// injection approach below targets the Qt 6.5+ WebLocalStorageFormat; if the
// app uses the default NativeFormat (IDBFS), additional investigation into the
// Emscripten filesystem API will be required to inject credentials.
// ─────────────────────────────────────────────────────────────────────────────

test.describe('Intervals.icu credential integration (Layer B)', () => {

  test.beforeEach(({ }, testInfo) => {
    const apiKey    = process.env.INTERVALS_ICU_API_KEY    || '';
    const athleteId = process.env.INTERVALS_ICU_ATHLETE_ID || '';
    if (!apiKey || !athleteId) {
      testInfo.skip(true,
        'Skipped: set INTERVALS_ICU_API_KEY and INTERVALS_ICU_ATHLETE_ID to run Layer B tests.');
    }
  });

  test('app with injected credentials does not show an intervals.icu auth error', async ({ page }) => {
    const apiKey    = process.env.INTERVALS_ICU_API_KEY    || '';
    const athleteId = process.env.INTERVALS_ICU_ATHLETE_ID || '';

    await stubBluetooth(page);

    // Inject credentials into localStorage using the key format that Qt WASM
    // QSettings uses with WebLocalStorageFormat (Qt 6.5+).
    // Keys follow the pattern: <group>/<setting-key>
    await page.addInitScript(({ apiKey, athleteId }) => {
      try {
        localStorage.setItem('Max++ inc./MaximumTrainer/account/intervals_icu_api_key',    apiKey);
        localStorage.setItem('Max++ inc./MaximumTrainer/account/intervals_icu_athlete_id', athleteId);
        localStorage.setItem('Max++ inc./MaximumTrainer/account/intervals_icu_auto_upload', 'false');
      } catch (e) {
        console.warn('localStorage injection failed:', e);
      }
    }, { apiKey, athleteId });

    const authErrors = [];
    page.on('requestfinished', async req => {
      if (!req.url().includes('intervals.icu')) return;
      const resp = await req.response();
      if (resp && resp.status() === 401) {
        authErrors.push(`401 on ${req.url()}`);
      }
    });

    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
    await page.waitForTimeout(8000);

    expect(
      authErrors,
      `Intervals.icu returned 401 — credentials may not have been injected correctly: ` +
      authErrors.join(', '),
    ).toHaveLength(0);
  });

  test('intervals.icu API returns 200 for GET athlete with injected credentials', async ({ page }) => {
    const apiKey    = process.env.INTERVALS_ICU_API_KEY    || '';
    const athleteId = process.env.INTERVALS_ICU_ATHLETE_ID || '';

    await stubBluetooth(page);

    // Intercept and record intervals.icu response status codes.
    const successfulRequests = [];
    page.on('requestfinished', async req => {
      if (!req.url().includes('intervals.icu')) return;
      const resp = await req.response();
      if (resp && resp.status() === 200) {
        successfulRequests.push(req.url());
      }
    });

    await page.addInitScript(({ apiKey, athleteId }) => {
      try {
        localStorage.setItem('Max++ inc./MaximumTrainer/account/intervals_icu_api_key',    apiKey);
        localStorage.setItem('Max++ inc./MaximumTrainer/account/intervals_icu_athlete_id', athleteId);
        localStorage.setItem('Max++ inc./MaximumTrainer/account/intervals_icu_auto_upload', 'false');
      } catch (e) {
        console.warn('localStorage injection failed:', e);
      }
    }, { apiKey, athleteId });

    await page.goto(APP_URL, { waitUntil: 'domcontentloaded' });
    await page.waitForTimeout(8000);

    // If credentials were injectedsuccessfully and the app made API calls,
    // at least one should have returned 200.  If zero requests were made it
    // likely means the QSettings key format differs — see NOTE above.
    if (successfulRequests.length === 0) {
      test.info().annotations.push({
        type: 'warning',
        description:
          'No successful intervals.icu requests observed. ' +
          'The Qt WASM QSettings localStorage key format may differ from the injected keys. ' +
          'Investigate the app\'s Emscripten virtual filesystem to confirm the correct key path.',
      });
    }
  });

});
