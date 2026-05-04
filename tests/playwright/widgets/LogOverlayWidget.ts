import { type Locator, type Page } from '@playwright/test';

/**
 * Widget wrapping the `#wasm-log-overlay` diagnostic panel populated by
 * `logger.js`.  Every `console.log`, `console.warn`, and `console.error`
 * call from the WASM module is captured here as individual `<div>` entries.
 */
export class LogOverlayWidget {
  /** Root `<div id="wasm-log-overlay">` container */
  readonly root: Locator;
  /** `<button id="wasm-log-copy-btn">` — copies all log text to the clipboard */
  readonly copyButton: Locator;

  private readonly page: Page;

  constructor(page: Page) {
    this.page       = page;
    this.root       = page.locator('#wasm-log-overlay');
    this.copyButton = page.locator('#wasm-log-copy-btn');
  }

  /**
   * Locator for all individual log line `<div>` elements inside the overlay.
   * Use `logLines().first()` or `expect(logLines()).toHaveCount(n)` to assert
   * on specific entries.
   */
  logLines(): Locator {
    return this.root.locator('div > div');
  }

  /**
   * Returns all `ERROR:` log lines from the overlay, excluding known
   * network noise from the test environment:
   *   - `net::ERR_CONNECTION_REFUSED`  — maximumtrainer.com backend unavailable
   *   - `Refused to set unsafe header` — browser rejects `User-Agent` on XHR
   */
  async getFatalErrorLines(): Promise<string[]> {
    const allErrorLines: string[] = await this.page.evaluate(() => {
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
   * Returns all log lines whose text contains the given substring.
   * @param text  Substring to search for (case-sensitive)
   */
  async getLinesContaining(text: string): Promise<string[]> {
    return this.page.evaluate((searchText: string) => {
      const logContent = document.querySelector('#wasm-log-overlay > div:last-child');
      if (!logContent) return [];
      return Array.from(logContent.querySelectorAll('div'))
        .map((d) => d.textContent ?? '')
        .filter((t) => t.includes(searchText));
    }, text);
  }
}
