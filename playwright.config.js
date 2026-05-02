// @ts-check
const { defineConfig, devices } = require('@playwright/test');

module.exports = defineConfig({
  testDir: './tests/playwright',
  timeout: 60_000,
  retries: 1,
  reporter: [
    ['list'],
    // HTML report written to playwright-report/ — uploaded as a CI artifact
    ['html', { open: 'never' }],
  ],
  use: {
    // Run in headless Chromium — WebBluetooth stub is injected per-test
    ...devices['Desktop Chrome'],
    headless: true,
    // Accept self-signed certs in case GitHub Pages has edge issues
    ignoreHTTPSErrors: true,
    // Capture a screenshot automatically on test failure for easier debugging
    screenshot: 'only-on-failure',
    launchOptions: {
      args: [
        // Force V8 to use only the Liftoff baseline WASM compiler (no Turbofan
        // tier-up).  The 18 MB ASYNCIFY-transformed WASM binary can take
        // several minutes for Turbofan to compile on cold CI runners; Liftoff
        // compiles the same binary in seconds at the cost of slower runtime
        // performance — which is irrelevant for test assertions.
        '--js-flags=--no-wasm-tier-up',
      ],
    },
  },
});
