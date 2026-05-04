import { type Locator, type Page } from '@playwright/test';

/**
 * Widget wrapping the `#browser-warning` compatibility banner that is
 * displayed when the browser does not meet the Web Bluetooth requirements
 * (API absent, insecure context, or no Bluetooth hardware detected).
 */
export class BrowserCompatibilityWidget {
  /** Root `<div id="browser-warning">` container */
  readonly root: Locator;
  /** Detail paragraph `<p id="browser-warning-detail">` describing the issue */
  readonly detail: Locator;
  /** "← Back to site" anchor link */
  readonly backLink: Locator;

  constructor(page: Page) {
    this.root     = page.locator('#browser-warning');
    this.detail   = page.locator('#browser-warning-detail');
    this.backLink = this.root.locator('a');
  }

  /** `true` when the compatibility warning is currently visible. */
  async isVisible(): Promise<boolean> {
    return this.root.isVisible();
  }

  /** Returns the full text content of the detail paragraph. */
  async getDetailText(): Promise<string> {
    return (await this.detail.textContent()) ?? '';
  }
}
