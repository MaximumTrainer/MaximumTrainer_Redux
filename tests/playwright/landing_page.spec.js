// @ts-check
const { test, expect, request } = require('@playwright/test');

const SITE_URL  = 'https://maximumtrainer.github.io/MaximumTrainer_Redux/';
const BASE_PATH = 'https://maximumtrainer.github.io/MaximumTrainer_Redux';

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
    await page.goto(SITE_URL, { waitUntil: 'domcontentloaded' });
    await expect(page).toHaveTitle(/Maximum Trainer/i);
  });

  test('hero section is visible', async ({ page }) => {
    await page.goto(SITE_URL, { waitUntil: 'domcontentloaded' });
    const hero = page.locator('#hero');
    await expect(hero).toBeVisible();
  });

  test('"Try in Browser" link points to the WASM app', async ({ page }) => {
    await page.goto(SITE_URL, { waitUntil: 'domcontentloaded' });

    // The CTA button that links to the WASM app must be present and have the
    // correct href so post-deployment the browser app is discoverable.
    const tryInBrowserLink = page.locator('a[href="app/"]');
    await expect(tryInBrowserLink,
      '"Try in Browser" link (href="app/") should exist in the hero section')
      .toHaveCount(1);
  });

  test('navigation links are present', async ({ page }) => {
    await page.goto(SITE_URL, { waitUntil: 'domcontentloaded' });

    const nav = page.locator('nav');
    await expect(nav).toBeVisible();

    // Core nav items expected on every deployment
    for (const label of ['Features', 'Download', 'User Guide']) {
      await expect(
        page.locator(`nav a:has-text("${label}")`).first(),
        `Nav link "${label}" should be present`,
      ).toBeVisible();
    }
  });

  test('features section is present', async ({ page }) => {
    await page.goto(SITE_URL, { waitUntil: 'domcontentloaded' });
    const features = page.locator('#features');
    await expect(features).toBeVisible();
  });

  test('download section is present', async ({ page }) => {
    await page.goto(SITE_URL, { waitUntil: 'domcontentloaded' });
    const download = page.locator('#download');
    await expect(download).toBeVisible();
  });

  test('no critical console errors on page load', async ({ page }) => {
    const criticalErrors = [];
    page.on('console', msg => {
      if (msg.type() === 'error') {
        criticalErrors.push(msg.text());
      }
    });
    page.on('pageerror', err => criticalErrors.push(err.message));

    await page.goto(SITE_URL, { waitUntil: 'domcontentloaded' });
    await page.waitForTimeout(2000);

    expect(
      criticalErrors,
      `Unexpected console errors on landing page: ${criticalErrors.join(', ')}`,
    ).toHaveLength(0);
  });

  test('GitHub repository link is present', async ({ page }) => {
    await page.goto(SITE_URL, { waitUntil: 'domcontentloaded' });

    const ghLink = page.locator('a[href*="github.com/MaximumTrainer/MaximumTrainer_Redux"]').first();
    await expect(ghLink, 'GitHub repo link should be present on the landing page').toBeVisible();
  });
});
