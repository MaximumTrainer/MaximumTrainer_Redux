import { defineConfig, devices } from '@playwright/test';

/**
 * MaximumTrainer Playwright Configuration
 *
 * Projects:
 *  wasm-chromium    – All WASM app tests; Chromium required for Web Bluetooth.
 *  landing-chromium – Landing-page / asset tests on Chromium.
 *  landing-firefox  – Landing-page tests on Firefox (optional; not in CI).
 *  landing-webkit   – Landing-page tests on WebKit/Safari (optional; not in CI).
 *
 * The WASM binary (~18 MB, ASYNCIFY-transformed) compiles in seconds under
 * Liftoff (--no-wasm-tier-up) vs. several minutes under Turbofan on cold CI
 * runners.  All WASM projects include this flag.
 *
 * Multi-browser landing projects are omitted in CI environments (where only
 * Chromium is installed) to avoid "executable doesn't exist" failures.
 * Set CI=true (GitHub Actions does this automatically) to suppress them.
 */

const WASM_CHROMIUM_ARGS: string[] = [
  // Force V8 to use only the Liftoff baseline WASM compiler (no Turbofan
  // tier-up).  Cuts cold-start compile time from 3+ min to ~30 s.
  '--js-flags=--no-wasm-tier-up',
];

// Firefox and WebKit landing-page projects are only registered outside CI
// (GitHub Actions sets CI=true automatically).
const multiB = !process.env['CI']
  ? [
      {
        name: 'landing-firefox',
        use: { ...devices['Desktop Firefox'] },
        testMatch: /landing_page\.spec\.(ts|js)$/,
      },
      {
        name: 'landing-webkit',
        use: { ...devices['Desktop Safari'] },
        testMatch: /landing_page\.spec\.(ts|js)$/,
      },
    ]
  : [];

export default defineConfig({
  testDir: './tests/playwright',

  // Default per-test timeout.  WASM tests override this via
  // test.describe.configure({ timeout }) inside the spec file.
  timeout: 60_000,

  retries: 1,

  reporter: [
    ['list'],
    // HTML report written to playwright-report/ — uploaded as a CI artifact.
    ['html', { open: 'never' }],
  ],

  use: {
    headless: true,
    // Accept self-signed certs in case GitHub Pages has edge issues.
    ignoreHTTPSErrors: true,
    // Capture a screenshot automatically on test failure for easier debugging.
    screenshot: 'only-on-failure',
  },

  projects: [
    // ── WASM app tests ────────────────────────────────────────────────────
    // Chromium is required because Web Bluetooth is only available in
    // Chrome/Chromium-based browsers.
    {
      name: 'wasm-chromium',
      use: {
        ...devices['Desktop Chrome'],
        launchOptions: { args: WASM_CHROMIUM_ARGS },
      },
      testMatch: /wasm_.*\.spec\.(ts|js)$|intervals_icu\.spec\.(ts|js)$/,
    },

    // ── Landing page / asset checks ───────────────────────────────────────
    {
      name: 'landing-chromium',
      use: { ...devices['Desktop Chrome'] },
      testMatch: /landing_page\.spec\.(ts|js)$/,
    },

    // Optional multi-browser landing page projects (disabled in CI).
    ...multiB,
  ],
});
