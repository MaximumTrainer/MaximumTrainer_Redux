# Instructions

- Following Playwright test failed.
- Explain why, be concise, respect Playwright best practices.
- Provide a snippet of code with the fix, if possible.

# Test info

- Name: landing_page.spec.js >> Landing page content >> features section is present
- Location: tests/playwright/landing_page.spec.js:64:3

# Error details

```
Error: page.goto: net::ERR_NAME_NOT_RESOLVED at https://maximumtrainer.github.io/MaximumTrainer_Redux/
Call log:
  - navigating to "https://maximumtrainer.github.io/MaximumTrainer_Redux/", waiting until "domcontentloaded"

```

# Test source

```ts
  1   | // @ts-check
  2   | const { test, expect, request } = require('@playwright/test');
  3   | 
  4   | const SITE_URL  = 'https://maximumtrainer.github.io/MaximumTrainer_Redux/';
  5   | const BASE_PATH = 'https://maximumtrainer.github.io/MaximumTrainer_Redux';
  6   | 
  7   | // ── HTTP asset checks ──────────────────────────────────────────────────────
  8   | test.describe('Landing page assets are deployed', () => {
  9   |   test('index.html returns 200', async ({ request }) => {
  10  |     const res = await request.get(SITE_URL);
  11  |     expect(res.status(), 'Landing page should be reachable (200)').toBe(200);
  12  |   });
  13  | 
  14  |   test('CSS stylesheet returns 200', async ({ request }) => {
  15  |     const res = await request.get(`${BASE_PATH}/css/style.css`);
  16  |     expect(res.status(), 'css/style.css should be deployed (200)').toBe(200);
  17  |   });
  18  | 
  19  |   test('favicon / icon returns 200', async ({ request }) => {
  20  |     const res = await request.get(`${BASE_PATH}/assets/images/main_icon.png`);
  21  |     expect(res.status(), 'assets/images/main_icon.png should be deployed (200)').toBe(200);
  22  |   });
  23  | });
  24  | 
  25  | // ── Page-level checks ──────────────────────────────────────────────────────
  26  | test.describe('Landing page content', () => {
  27  |   test('page has the correct title', async ({ page }) => {
  28  |     await page.goto(SITE_URL, { waitUntil: 'domcontentloaded' });
  29  |     await expect(page).toHaveTitle(/Maximum Trainer/i);
  30  |   });
  31  | 
  32  |   test('hero section is visible', async ({ page }) => {
  33  |     await page.goto(SITE_URL, { waitUntil: 'domcontentloaded' });
  34  |     const hero = page.locator('#hero');
  35  |     await expect(hero).toBeVisible();
  36  |   });
  37  | 
  38  |   test('"Try in Browser" link points to the WASM app', async ({ page }) => {
  39  |     await page.goto(SITE_URL, { waitUntil: 'domcontentloaded' });
  40  | 
  41  |     // The CTA button that links to the WASM app must be present and have the
  42  |     // correct href so post-deployment the browser app is discoverable.
  43  |     const tryInBrowserLink = page.locator('a[href="app/"]');
  44  |     await expect(tryInBrowserLink,
  45  |       '"Try in Browser" link (href="app/") should exist in the hero section')
  46  |       .toHaveCount(1);
  47  |   });
  48  | 
  49  |   test('navigation links are present', async ({ page }) => {
  50  |     await page.goto(SITE_URL, { waitUntil: 'domcontentloaded' });
  51  | 
  52  |     const nav = page.locator('nav');
  53  |     await expect(nav).toBeVisible();
  54  | 
  55  |     // Core nav items expected on every deployment
  56  |     for (const label of ['Features', 'Download', 'User Guide']) {
  57  |       await expect(
  58  |         page.locator(`nav a:has-text("${label}")`).first(),
  59  |         `Nav link "${label}" should be present`,
  60  |       ).toBeVisible();
  61  |     }
  62  |   });
  63  | 
  64  |   test('features section is present', async ({ page }) => {
> 65  |     await page.goto(SITE_URL, { waitUntil: 'domcontentloaded' });
      |                ^ Error: page.goto: net::ERR_NAME_NOT_RESOLVED at https://maximumtrainer.github.io/MaximumTrainer_Redux/
  66  |     const features = page.locator('#features');
  67  |     await expect(features).toBeVisible();
  68  |   });
  69  | 
  70  |   test('download section is present', async ({ page }) => {
  71  |     await page.goto(SITE_URL, { waitUntil: 'domcontentloaded' });
  72  |     const download = page.locator('#download');
  73  |     await expect(download).toBeVisible();
  74  |   });
  75  | 
  76  |   test('no critical console errors on page load', async ({ page }) => {
  77  |     const criticalErrors = [];
  78  |     page.on('console', msg => {
  79  |       if (msg.type() === 'error') {
  80  |         criticalErrors.push(msg.text());
  81  |       }
  82  |     });
  83  |     page.on('pageerror', err => criticalErrors.push(err.message));
  84  | 
  85  |     await page.goto(SITE_URL, { waitUntil: 'domcontentloaded' });
  86  |     await page.waitForTimeout(2000);
  87  | 
  88  |     expect(
  89  |       criticalErrors,
  90  |       `Unexpected console errors on landing page: ${criticalErrors.join(', ')}`,
  91  |     ).toHaveLength(0);
  92  |   });
  93  | 
  94  |   test('GitHub repository link is present', async ({ page }) => {
  95  |     await page.goto(SITE_URL, { waitUntil: 'domcontentloaded' });
  96  | 
  97  |     const ghLink = page.locator('a[href*="github.com/MaximumTrainer/MaximumTrainer_Redux"]').first();
  98  |     await expect(ghLink, 'GitHub repo link should be present on the landing page').toBeVisible();
  99  |   });
  100 | });
  101 | 
```