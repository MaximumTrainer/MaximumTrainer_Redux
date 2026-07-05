/**
 * Log Out survival – WASM (regression for #344)
 *
 * Log Out used to abort the whole WASM runtime ("uncaught RuntimeError")
 * because the confirmation box ran QDialog::exec(), which is a qFatal on
 * Qt for WebAssembly without asyncify. The fix confirms via QDialog::open()
 * and this suite proves the full cycle survives in-browser:
 *
 *   1. Mocked OAuth login → MainWindow (same setup as wasm_workout_render).
 *   2. window.mt_triggerLogout() runs the confirmed logout path: credentials
 *      cleared, MainWindow saved + torn down, DialogLogin re-shown in-process.
 *   3. The login dialog re-appearing (mt_wasmOAuthReady re-set by its
 *      constructor) and a second successful login prove the runtime is alive.
 *
 * The round-1 JS hooks are explicitly cleared before logout so their
 * reappearance can only come from the second DialogLogin / MainWindow
 * construction, not from stale registrations.
 */

import { test, expect, type BrowserContext } from '@playwright/test';
import { WasmAppPage } from './pages/WasmAppPage';

const SCREENSHOT_DIR = 'test-results/wasm-screenshots';

test.describe('WASM Log Out returns to the login screen without crashing', () => {
  test.describe.configure({ timeout: 420_000 });

  let ctx: BrowserContext;
  let wasmApp: WasmAppPage;
  const pageErrors: string[] = [];

  test.beforeAll(async ({ browser }) => {
    test.setTimeout(420_000);
    ctx = await browser.newContext();
    wasmApp = new WasmAppPage(await ctx.newPage());
    wasmApp.page.on('pageerror', (err) => pageErrors.push(err.message));

    await wasmApp.stubBluetooth();
    await wasmApp.disableIcuProxyInterceptor();
    await wasmApp.mockIntervalsIcuApi();
    // setupOAuthMock must follow mockIntervalsIcuApi so its specific routes
    // take priority (Playwright tries routes last-first).
    await wasmApp.setupOAuthMock();

    await wasmApp.goto();
    await wasmApp.waitForFullyLoaded(300_000);

    // Round 1: fully-mocked OAuth login → MainWindow.
    await wasmApp.completeOAuthLogin(180_000);
    await wasmApp.waitForDemoWorkoutHook(60_000);
  });

  test.afterAll(async () => {
    await ctx.close();
  });

  test('Preferences opens without killing the runtime', async () => {
    // dconfig->exec() used to be a fatal nested event loop on WASM — opening
    // Preferences aborted the app just like Log Out did (#344).
    await wasmApp.page.evaluate(() => (window as any).mt_openPreferences());
    await wasmApp.page.waitForTimeout(3_000);
    await wasmApp.page.screenshot({ path: `${SCREENSHOT_DIR}/preferences-open.png` });

    expect(
      pageErrors,
      `Uncaught page errors after opening Preferences:\n${pageErrors.join('\n')}`,
    ).toHaveLength(0);

    // Close it again (Esc) so the logout test below starts from MainWindow.
    await wasmApp.page.keyboard.press('Escape');
    await wasmApp.page.waitForTimeout(1_000);
  });

  test('logout tears down MainWindow and re-shows the login dialog', async () => {
    // Clear the round-1 bridge flags so their reappearance below can only be
    // caused by the login dialog / main window being constructed again.
    await wasmApp.page.evaluate(() => {
      (window as any).mt_wasmOAuthReady = false;
      delete (window as any).mt_setIntervalsCredentials;
      delete (window as any).mt_intervalsRefresh;
    });

    await wasmApp.page.evaluate(() => (window as any).mt_triggerLogout());

    // DialogLogin's constructor re-sets mt_wasmOAuthReady — the signal that
    // the teardown survived and the app is back at the login screen.
    await wasmApp.page.waitForFunction(
      () => (window as any).mt_wasmOAuthReady === true,
      null,
      { timeout: 120_000 },
    );
    await wasmApp.page.screenshot({ path: `${SCREENSHOT_DIR}/logout-login-screen.png` });
  });

  test('a second OAuth login reaches MainWindow again', async () => {
    await wasmApp.completeOAuthLogin(180_000);
    await wasmApp.page.screenshot({ path: `${SCREENSHOT_DIR}/logout-relogin.png` });
  });

  test('no uncaught runtime errors or fatal log lines across the cycle', async () => {
    expect(
      pageErrors,
      `Uncaught page errors during logout/login cycle:\n${pageErrors.join('\n')}`,
    ).toHaveLength(0);

    const fatalLines = await wasmApp.logOverlay.getFatalErrorLines();
    expect(
      fatalLines,
      `Fatal log-overlay lines during logout/login cycle:\n${fatalLines.join('\n')}`,
    ).toHaveLength(0);
  });
});
