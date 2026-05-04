/**
 * Shared helpers for WASM Playwright test suites — TypeScript edition.
 *
 * **Backward compatibility shim only.**  This module re-exports the same
 * function signatures as the original `wasm-test-helpers.js`, but now
 * delegates internally to `WasmAppPage` / `LandingPage` page objects.
 *
 * **New test files should NOT import from this module.**  Instead, import
 * page objects directly:
 * ```typescript
 * import { WasmAppPage } from './pages/WasmAppPage';
 * import { LandingPage } from './pages/LandingPage';
 * ```
 * and call their methods (`wasmApp.stubBluetooth()`, `wasmApp.mockBackendApis()`,
 * `wasmApp.logOverlay.getFatalErrorLines()`, etc.).
 */

import { type Page } from '@playwright/test';
import { WasmAppPage } from './pages/WasmAppPage';

/**
 * Stub `navigator.bluetooth` so the WASM app does not abort on browsers
 * without Web Bluetooth support (e.g. headless Chromium in CI).
 *
 * @deprecated **Backward-compat shim.** New tests should use:
 * ```typescript
 * const wasmApp = new WasmAppPage(page);
 * await wasmApp.stubBluetooth();
 * ```
 */
export async function stubBluetooth(page: Page): Promise<void> {
  return new WasmAppPage(page).stubBluetooth();
}

/**
 * Wait for the WASM Qt app to finish loading (canvas visible or loading
 * screen hidden).
 *
 * @deprecated Prefer `new WasmAppPage(page).waitForFullyLoaded(timeoutMs)`.
 */
export async function waitForAppReady(page: Page, timeoutMs = 60_000): Promise<void> {
  return new WasmAppPage(page).waitForFullyLoaded(timeoutMs);
}

/**
 * Mock Intervals.icu API endpoints with CORS preflight support.
 *
 * @deprecated Prefer `new WasmAppPage(page).mockIntervalsIcuApi()`.
 */
export async function mockIntervalsIcuApi(page: Page): Promise<string[]> {
  return new WasmAppPage(page).mockIntervalsIcuApi();
}

/**
 * Read all `ERROR:` log entries from `#wasm-log-overlay`, excluding known
 * network noise from the test environment.
 *
 * @deprecated Prefer `new WasmAppPage(page).logOverlay.getFatalErrorLines()`.
 */
export async function getOverlayFatalErrorLines(page: Page): Promise<string[]> {
  const allErrorLines: string[] = await page.evaluate(() => {
    const logContent = document.querySelector('#wasm-log-overlay > div:last-child');
    if (!logContent) return [];
    return Array.from(logContent.querySelectorAll('div'))
      .map((d) => d.textContent ?? '')
      .filter((t) => t.includes('ERROR:'));
  });

  return allErrorLines.filter(
    (line) =>
      !line.includes('net::ERR_CONNECTION_REFUSED') &&
      !line.includes('Refused to set unsafe header'),
  );
}

/**
 * Return all overlay log entries whose text contains the given substring.
 *
 * @deprecated Prefer `new WasmAppPage(page).logOverlay.getLinesContaining(text)`.
 */
export async function getOverlayLinesContaining(
  page: Page,
  text: string,
): Promise<string[]> {
  return page.evaluate((searchText: string) => {
    const logContent = document.querySelector('#wasm-log-overlay > div:last-child');
    if (!logContent) return [];
    return Array.from(logContent.querySelectorAll('div'))
      .map((d) => d.textContent ?? '')
      .filter((t) => t.includes(searchText));
  }, text);
}

/**
 * Mock `maximumtrainer.com` backend APIs.
 *
 * @deprecated Prefer `new WasmAppPage(page).mockBackendApis()`.
 */
export async function mockBackendApis(page: Page): Promise<string[]> {
  return new WasmAppPage(page).mockBackendApis();
}
