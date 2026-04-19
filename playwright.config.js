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
  },
});
