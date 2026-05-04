import { type Locator, type Page } from '@playwright/test';

/**
 * Widget wrapping the `#loading-screen` overlay shown while the WASM binary
 * is being fetched, compiled, and initialised.
 *
 * The loading screen transitions to hidden when the Qt `onLoaded` callback
 * fires inside `index.html`, at which point it receives the `hidden` CSS
 * class (sets `opacity: 0; pointer-events: none`).
 */
export class LoadingScreenWidget {
  /** Root `<div id="loading-screen">` container */
  readonly root: Locator;
  /** Animated progress bar fill `<div id="loading-progress">` */
  readonly progressBar: Locator;
  /** Status text label `<div id="loading-status">` */
  readonly status: Locator;
  /** App title label `<div id="loading-title">` */
  readonly title: Locator;

  constructor(page: Page) {
    this.root        = page.locator('#loading-screen');
    this.progressBar = page.locator('#loading-progress');
    this.status      = page.locator('#loading-status');
    this.title       = page.locator('#loading-title');
  }

  /** `true` when the loading screen is currently visible. */
  async isVisible(): Promise<boolean> {
    return this.root.isVisible();
  }

  /**
   * `true` when the loading screen has been dismissed by the `hidden` CSS
   * class (set by the `onLoaded` callback after WASM initialises).
   */
  async hasHiddenClass(): Promise<boolean> {
    const cls = (await this.root.getAttribute('class')) ?? '';
    return cls.includes('hidden');
  }
}
