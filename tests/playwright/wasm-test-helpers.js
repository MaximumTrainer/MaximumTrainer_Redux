// @ts-check
/**
 * Shared helpers for WASM Playwright test suites.
 */

/**
 * Read all log entries from the #wasm-log-overlay that begin with 'ERROR:'.
 *
 * logger.js appends each entry as a <div> child of the logContent <div>
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

module.exports = { getOverlayFatalErrorLines };
