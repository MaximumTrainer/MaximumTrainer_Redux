// @ts-check
/**
 * Shared helpers for WASM Playwright test suites.
 */

/**
 * Stub navigator.bluetooth so the WASM app doesn't abort on browsers without
 * Web Bluetooth support (e.g. headless Chromium in CI).
 *
 * @param {import('@playwright/test').Page} page
 */
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

/**
 * Wait for the WASM Qt app to finish loading (canvas visible).
 *
 * @param {import('@playwright/test').Page} page
 * @param {number} [timeoutMs=60000]
 */
async function waitForAppReady(page, timeoutMs = 60_000) {
  await page.waitForFunction(() => {
    const canvas = document.querySelector('#qt-canvas-wrapper');
    return canvas && getComputedStyle(canvas).visibility !== 'hidden';
  }, null, { timeout: timeoutMs });
}

/**
 * Mock Intervals.icu API endpoints, including CORS preflight (OPTIONS) support.
 * Returns an array that accumulates `"METHOD URL"` strings for intercepted requests.
 *
 * Must be called BEFORE page.goto().
 *
 * @param {import('@playwright/test').Page} page
 * @returns {Promise<string[]>} Live array of intercepted request strings.
 */
async function mockIntervalsIcuApi(page) {
  const requestedUrls = [];

  await page.route('https://intervals.icu/**', async (route) => {
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
            id: 'evt001',
            name: 'Playwright Test Workout',
            start_date_local: new Date().toISOString().split('T')[0],
            type: 'Ride',
            moving_time: 3600,
            workout_id: 'wk001',
            description: 'Test workout for Playwright',
          },
        ]),
      });
    } else if (url.includes('/workouts/') && url.endsWith('.zwo')) {
      // Return a minimal valid ZWO file
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

/**
 * Read all log entries from the #wasm-log-overlay that begin with 'ERROR:'.
 * (itself a direct child of #wasm-log-overlay, after the copy button).
 * We query DOM nodes rather than splitting textContent so we get one entry
 * per line regardless of whether the text contains newlines.
 *
 * Network errors that are expected in the test environment (backend
 * unavailable, browser security restrictions) are excluded so the assertion
 * does not fire on noise unrelated to WASM loading:
 *   - net::ERR_CONNECTION_REFUSED  — maximumtrainer.com backend not available
 *   - Refused to set unsafe header — browser rejects User-Agent on XHR
 *
 * @param {import('@playwright/test').Page} page
 * @returns {Promise<string[]>} ERROR lines that are not excluded
 */
async function getOverlayFatalErrorLines(page) {
  const allErrorLines = await page.evaluate(() => {
    // logContent is the last <div> child of the overlay (after the <button>)
    const logContent = document.querySelector('#wasm-log-overlay > div:last-child');
    if (!logContent) return [];
    return Array.from(logContent.querySelectorAll('div'))
      .map(d => d.textContent || '')
      .filter(t => t.includes('ERROR:'));
  });

  return allErrorLines.filter(line =>
    !line.includes('net::ERR_CONNECTION_REFUSED') &&  // backend unavailable in test env
    !line.includes('Refused to set unsafe header')    // browser security on XHR User-Agent
  );
}

/**
 * Return all overlay log entries whose text contains the given substring.
 *
 * @param {import('@playwright/test').Page} page
 * @param {string} text  Substring to search for (case-sensitive)
 * @returns {Promise<string[]>}
 */
async function getOverlayLinesContaining(page, text) {
  return page.evaluate((searchText) => {
    const logContent = document.querySelector('#wasm-log-overlay > div:last-child');
    if (!logContent) return [];
    return Array.from(logContent.querySelectorAll('div'))
      .map(d => d.textContent || '')
      .filter(t => t.includes(searchText));
  }, text);
}

/**
 * Register Playwright route intercepts for all maximumtrainer.com backend
 * API endpoints.  Returns the request at the CDP network layer — before the
 * browser enforces CORS — so Qt WASM's XHR calls succeed even when the
 * backend is unreachable in the test environment.
 *
 * Must be called BEFORE page.goto() so the intercept is in place before the
 * app starts.
 *
 * Both radio and achievement parsers (parseJsonRadioList /
 * parseJsonAchievementList) expect a top-level JSON array.  Returning '[]'
 * gives a 200 OK with an empty list — the app accepts this gracefully with no
 * retry and no WARN log line.  All other maximumtrainer.com routes get a
 * generic '{}' 200 response to prevent unhandled errors.
 *
 * @param {import('@playwright/test').Page} page
 * @returns {string[]} Live array of `"${METHOD} ${url}"` strings that is
 *   populated as requests arrive.  Callers can assert expected endpoints were
 *   actually hit after the app has loaded.
 */
async function mockBackendApis(page) {
  const requestedUrls = [];

  await page.route('https://maximumtrainer.com/**', async (route) => {
    const url    = route.request().url();
    const method = route.request().method();
    requestedUrls.push(`${method} ${url}`);

    if (method === 'OPTIONS') {
      // Satisfy any CORS preflight that the browser may emit for cross-origin XHR.
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

module.exports = { stubBluetooth, waitForAppReady, mockIntervalsIcuApi, getOverlayFatalErrorLines, getOverlayLinesContaining, mockBackendApis };
