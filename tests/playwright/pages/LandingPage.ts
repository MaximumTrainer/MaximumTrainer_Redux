import { type Page, type Locator } from '@playwright/test';

const BASE_ORIGIN =
  process.env['PLAYWRIGHT_BASE_URL'] ||
  'https://maximumtrainer.github.io/MaximumTrainer_Redux';

/** Full URL of the marketing / landing page. */
export const SITE_URL  = `${BASE_ORIGIN}/`;
/** Base path for asset URL construction (no trailing slash). */
export const BASE_PATH = BASE_ORIGIN;

/**
 * Page Object for the MaximumTrainer marketing landing page (`docs/index.html`).
 *
 * Exposes locators for the primary navigation, hero section, content
 * sections, and social links.  All locators use stable IDs or ARIA roles;
 * class-based selectors are only used where IDs are absent.
 *
 * ## OS / cross-browser notes
 * - The landing page is plain HTML/CSS with no Web Bluetooth dependency,
 *   so it can be tested on Chromium, Firefox, and WebKit alike.
 * - There are no OS-specific keyboard shortcuts on this page.
 * - On mobile-viewport sizes the nav toggle button (`#navToggle`) must be
 *   clicked before nav links are visible — not required on desktop viewports.
 */
export class LandingPage {
  readonly page: Page;

  // ── Navigation bar ────────────────────────────────────────────────────────

  /** Primary `<nav class="nav-container">` inside the site header */
  readonly nav: Locator;

  // ── Hero section (#hero) ──────────────────────────────────────────────────

  /** Hero `<section id="hero">` */
  readonly hero: Locator;
  /** "Try in Browser" link inside the hero (`href="app/"`) */
  readonly tryInBrowserLink: Locator;
  /** Primary "Download" call-to-action button (currently disabled) */
  readonly downloadButton: Locator;
  /** "View on GitHub" link inside the hero */
  readonly githubHeroLink: Locator;

  // ── Content sections ──────────────────────────────────────────────────────

  /** Features section (`#features`) */
  readonly featuresSection: Locator;
  /** Download section (`#download`) */
  readonly downloadSection: Locator;

  // ── Global links ──────────────────────────────────────────────────────────

  /** First link pointing to the GitHub repository */
  readonly githubRepoLink: Locator;

  constructor(page: Page) {
    this.page = page;

    this.nav              = page.locator('nav.nav-container');
    this.hero             = page.locator('#hero');
    this.tryInBrowserLink = this.hero.locator('a[href="app/"]');
    this.downloadButton   = this.hero.locator('button.btn-primary');
    this.githubHeroLink   = this.hero.locator('a[href*="github.com"]');
    this.featuresSection  = page.locator('#features');
    this.downloadSection  = page.locator('#download');
    this.githubRepoLink   = page
      .locator('a[href*="github.com/MaximumTrainer/MaximumTrainer_Redux"]')
      .first();
  }

  /** Navigate to the landing page and wait for DOM content to load. */
  async goto(): Promise<void> {
    await this.page.goto(SITE_URL, { waitUntil: 'domcontentloaded' });
  }

  /**
   * Returns the nav `<a>` element matching the given visible label text.
   * Scoped to `.nav-container` to avoid matching footer links.
   */
  navLink(label: string): Locator {
    return this.nav.locator(`a:has-text("${label}")`);
  }

  /** Full URL of the landing page. */
  get url(): string {
    return SITE_URL;
  }

  /** Base path for constructing asset URLs. */
  get basePath(): string {
    return BASE_PATH;
  }
}
