/**
 * Metric Dashboard render verification – WASM
 *
 * Proves that the QML metric dashboard (a `QQuickWidget` embedded in the
 * workout view) actually composites into the Qt `<canvas>` when the app runs
 * in the browser — the one thing a successful `build_wasm` link does NOT prove.
 *
 * The whole Qt app paints into a single `<canvas>` that is opaque to DOM
 * locators, so the only render evidence is the pixels. This suite:
 *   1. Logs in (fully mocked OAuth) to reach MainWindow.
 *   2. Drives the app into the dashboard view via the `mt_startDemoWorkout`
 *      C++ test hook (simulator-fed, no BLE hardware needed).
 *   3. Captures canvas screenshots before/after and asserts, via the
 *      dependency-free pngjs decoder, that the view rendered (non-blank),
 *      changed when the workout opened, and is actively updating.
 *
 * Pixel-exact baselines are deliberately avoided (flaky across GL drivers and
 * font rasterizers). The saved screenshots are also uploaded as CI artifacts
 * for human confirmation that the dashboard band painted.
 */

import { test, expect, type BrowserContext } from '@playwright/test';
import { WasmAppPage } from './pages/WasmAppPage';
import { assertNotBlank } from './canvas-pixels';

const SCREENSHOT_DIR = 'test-results/wasm-screenshots';

test.describe('WASM metric dashboard renders in-browser', () => {
  test.describe.configure({ timeout: 420_000 });

  let ctx: BrowserContext;
  let wasmApp: WasmAppPage;
  let beforeShot: Buffer;
  let afterShot1: Buffer;
  let afterShot2: Buffer;

  test.beforeAll(async ({ browser }) => {
    test.setTimeout(420_000);
    // Taller-than-default viewport: the workout dialog opens at natural size
    // (~900px), so the metric dashboard band at its bottom falls below the
    // standard 720px viewport. A 1080px-tall canvas keeps the band in frame.
    ctx = await browser.newContext({ viewport: { width: 1280, height: 1080 } });
    wasmApp = new WasmAppPage(await ctx.newPage());

    await wasmApp.stubBluetooth();
    await wasmApp.mockBackendApis();
    await wasmApp.mockIntervalsIcuApi();
    // setupOAuthMock must follow mockIntervalsIcuApi so its specific /oauth/token
    // and /athlete routes take priority (Playwright tries routes last-first).
    await wasmApp.setupOAuthMock();

    await wasmApp.goto();
    await wasmApp.waitForFullyLoaded(300_000);

    // Fully-mocked OAuth login → MainWindow (no real credentials needed).
    await wasmApp.completeOAuthLogin(180_000);

    // The demo-workout hook is registered in the MainWindow constructor.
    await wasmApp.waitForDemoWorkoutHook(60_000);

    // Baseline: the main-window view before entering the workout.
    await wasmApp.page.waitForTimeout(1_500);
    beforeShot = await wasmApp.screenshotCanvas();
    await wasmApp.page.screenshot({ path: `${SCREENSHOT_DIR}/dashboard-before.png` });

    // Enter the dashboard view; let QML compose and the simulator feed settle.
    await wasmApp.startDemoWorkout();
    await wasmApp.page.waitForTimeout(5_000);
    afterShot1 = await wasmApp.screenshotCanvas();
    await wasmApp.page.screenshot({ path: `${SCREENSHOT_DIR}/dashboard-running.png` });

    // Live updating: the simulator feeds metrics and the timer advances over 3s.
    await wasmApp.page.waitForTimeout(3_000);
    afterShot2 = await wasmApp.screenshotCanvas();
    await wasmApp.page.screenshot({ path: `${SCREENSHOT_DIR}/dashboard-running-2.png` });
  });

  test.afterAll(async () => { await ctx?.close(); });

  test('mt_startDemoWorkout hook is exposed by MainWindow', async () => {
    const hasHook = await wasmApp.page.evaluate(
      () => typeof (window as any).mt_startDemoWorkout === 'function',
    );
    expect(hasHook, 'window.mt_startDemoWorkout should be registered after login').toBe(true);
  });

  test('the dashboard view renders (canvas is not blank)', () => {
    const stats = assertNotBlank(afterShot1, 'dashboard view');
    // A real rendered view shows many distinct colours; a flat fill shows ~1.
    expect(stats.distinctColors).toBeGreaterThan(8);
  });

  test('entering the workout changes the rendered canvas', () => {
    expect(
      afterShot1.equals(beforeShot),
      'Canvas was byte-identical before and after launching the demo workout — ' +
      'the dashboard view did not render.',
    ).toBe(false);
  });

  test('the dashboard is live (canvas updates over time)', () => {
    expect(
      afterShot2.equals(afterShot1),
      'Canvas did not change over 3s of an active workout — the view is frozen ' +
      '(QML/metrics not driving the render).',
    ).toBe(false);
  });

  test('no fatal WASM errors during the dashboard session', async () => {
    const fatal = await wasmApp.logOverlay.getFatalErrorLines();
    expect(fatal, `Fatal WASM errors during dashboard render:\n${fatal.join('\n')}`).toHaveLength(0);
  });
});
