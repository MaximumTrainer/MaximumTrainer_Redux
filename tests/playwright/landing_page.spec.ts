import { test, expect } from '@playwright/test';
import { LandingPage, SITE_URL, BASE_PATH } from './pages/LandingPage';

// ── HTTP asset checks ──────────────────────────────────────────────────────
test.describe('Landing page assets are deployed', () => {
  test('index.html returns 200', async ({ request }) => {
    const res = await request.get(SITE_URL);
    expect(res.status(), 'Landing page should be reachable (200)').toBe(200);
  });

  test('CSS stylesheet returns 200', async ({ request }) => {
    const res = await request.get(`${BASE_PATH}/css/style.css`);
    expect(res.status(), 'css/style.css should be deployed (200)').toBe(200);
  });

  test('favicon / icon returns 200', async ({ request }) => {
    const res = await request.get(`${BASE_PATH}/assets/images/main_icon.png`);
    expect(res.status(), 'assets/images/main_icon.png should be deployed (200)').toBe(200);
  });
});

// ── Page-level checks ──────────────────────────────────────────────────────
test.describe('Landing page content', () => {
  test('page has the correct title', async ({ page }) => {
    const landingPage = new LandingPage(page);
    await landingPage.goto();
    await expect(page).toHaveTitle(/Maximum Trainer/i);
  });

  test('hero section is visible', async ({ page }) => {
    const landingPage = new LandingPage(page);
    await landingPage.goto();
    await expect(landingPage.hero).toBeVisible();
  });

  test('"Try in Browser" link points to the WASM app', async ({ page }) => {
    const landingPage = new LandingPage(page);
    await landingPage.goto();

    // The CTA button that links to the WASM app must be present and have the
    // correct href so post-deployment the browser app is discoverable.
    await expect(
      landingPage.tryInBrowserLink,
      '"Try in Browser" link (href="app/") should exist in the hero section',
    ).toHaveCount(1);
  });

  test('navigation links are present', async ({ page }) => {
    const landingPage = new LandingPage(page);
    await landingPage.goto();

    await expect(landingPage.nav).toBeVisible();

    // Core nav items expected on every deployment — scoped to the header nav
    // to avoid matching footer links (the page has two <nav> elements).
    for (const label of ['Features', 'Download', 'User Guide']) {
      await expect(
        landingPage.navLink(label),
        `Nav link "${label}" should be present in header nav`,
      ).toBeVisible();
    }
  });

  test('features section is present', async ({ page }) => {
    const landingPage = new LandingPage(page);
    await landingPage.goto();
    await expect(landingPage.featuresSection).toBeVisible();
  });

  test('download section is present', async ({ page }) => {
    const landingPage = new LandingPage(page);
    await landingPage.goto();
    await expect(landingPage.downloadSection).toBeVisible();
  });

  test('no critical console errors on page load', async ({ page }) => {
    const criticalErrors: string[] = [];
    page.on('console', (msg) => {
      if (msg.type() === 'error') {
        criticalErrors.push(msg.text());
      }
    });
    page.on('pageerror', (err) => criticalErrors.push(err.message));

    const landingPage = new LandingPage(page);
    await landingPage.goto();
    await page.waitForLoadState('networkidle');

    expect(
      criticalErrors,
      `Unexpected console errors on landing page: ${criticalErrors.join(', ')}`,
    ).toHaveLength(0);
  });

  test('GitHub repository link is present', async ({ page }) => {
    const landingPage = new LandingPage(page);
    await landingPage.goto();
    await expect(
      landingPage.githubRepoLink,
      'GitHub repo link should be present on the landing page',
    ).toBeVisible();
  });
});
