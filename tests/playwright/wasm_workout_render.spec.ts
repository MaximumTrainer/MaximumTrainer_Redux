/**
 * Workout view + retro-race render verification – WASM
 *
 * Proves the workout view paints into the Qt `<canvas>` in the browser, and —
 * the part a successful `build_wasm` link does NOT prove — that the QML retro
 * race (`RetroRace.qml`, which imports QtQuick.Shapes) actually composites.
 *
 * The whole Qt app paints into a single `<canvas>` that is opaque to DOM
 * locators, so the only render evidence is the pixels. This suite:
 *   1. Logs in (fully mocked OAuth) to reach MainWindow.
 *   2. Drives the app into a simulator-fed workout via `mt_startDemoWorkout`
 *      (no BLE hardware needed), then switches the content pane to the race
 *      via `mt_showRace`.
 *   3. Captures canvas screenshots and asserts, via the dependency-free pngjs
 *      decoder, that each view rendered (non-blank), changed on transition, and
 *      that the race loaded without QML errors.
 *
 * Pixel-exact baselines are deliberately avoided (flaky across GL drivers and
 * font rasterizers). The screenshots are uploaded as CI artifacts for human
 * confirmation.
 */

import { test, expect, type BrowserContext } from '@playwright/test';
import { WasmAppPage } from './pages/WasmAppPage';
import { assertNotBlank } from './canvas-pixels';

const SCREENSHOT_DIR = 'test-results/wasm-screenshots';

test.describe('WASM workout view + retro race render in-browser', () => {
  test.describe.configure({ timeout: 420_000 });

  let ctx: BrowserContext;
  let wasmApp: WasmAppPage;
  let beforeShot: Buffer;
  let workoutShot1: Buffer;
  let workoutShot2: Buffer;
  let raceShot: Buffer;
  let raceQmlErrors: string[] = [];

  test.beforeAll(async ({ browser }) => {
    test.setTimeout(420_000);
    // Taller-than-default viewport so the full workout dialog (~900px) is in frame.
    ctx = await browser.newContext({ viewport: { width: 1280, height: 1080 } });
    wasmApp = new WasmAppPage(await ctx.newPage());

    await wasmApp.stubBluetooth();
    await wasmApp.disableIcuProxyInterceptor();
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
    await wasmApp.page.screenshot({ path: `${SCREENSHOT_DIR}/workout-before.png` });

    // Enter the simulator-fed workout and let the widgets settle.
    await wasmApp.startDemoWorkout();
    await wasmApp.page.waitForTimeout(5_000);
    workoutShot1 = await wasmApp.screenshotCanvas();
    await wasmApp.page.screenshot({ path: `${SCREENSHOT_DIR}/workout-running.png` });

    // Live updating: the simulator feeds metrics over 3s.
    await wasmApp.page.waitForTimeout(3_000);
    workoutShot2 = await wasmApp.screenshotCanvas();
    await wasmApp.page.screenshot({ path: `${SCREENSHOT_DIR}/workout-running-2.png` });

    // Switch the content pane to the retro ghost-race (loads RetroRace.qml,
    // which imports QtQuick.Shapes) and let it compose.
    await wasmApp.showRace();
    await wasmApp.page.waitForTimeout(5_000);
    raceShot = await wasmApp.screenshotCanvas();
    await wasmApp.page.screenshot({ path: `${SCREENSHOT_DIR}/workout-race.png` });

    // Collect any QML load failures for the race component. A missing module
    // (e.g. QtQuick.Shapes not bundled in the wasm Qt build) surfaces here as
    // a QQuickWidget/QML error in the diagnostic log overlay.
    for (const needle of ['RetroRace', 'QtQuick.Shapes', 'is not installed', 'QQuickWidget']) {
      const lines = await wasmApp.logOverlay.getLinesContaining(needle);
      for (const l of lines)
        if (/error/i.test(l) || /not installed/i.test(l)) raceQmlErrors.push(l);
    }
  });

  test.afterAll(async () => { await ctx?.close(); });

  test('mt_startDemoWorkout hook is exposed by MainWindow', async () => {
    const hasHook = await wasmApp.page.evaluate(
      () => typeof (window as any).mt_startDemoWorkout === 'function',
    );
    expect(hasHook, 'window.mt_startDemoWorkout should be registered after login').toBe(true);
  });

  test('the workout view renders (canvas is not blank)', () => {
    const stats = assertNotBlank(workoutShot1, 'workout view');
    expect(stats.distinctColors).toBeGreaterThan(8);
  });

  test('entering the workout changes the rendered canvas', () => {
    expect(
      workoutShot1.equals(beforeShot),
      'Canvas was byte-identical before and after launching the demo workout — ' +
      'the workout view did not render.',
    ).toBe(false);
  });

  test('the workout view is live (canvas updates over time)', () => {
    expect(
      workoutShot2.equals(workoutShot1),
      'Canvas did not change over 3s of an active workout — the view is frozen ' +
      '(metrics not driving the render).',
    ).toBe(false);
  });

  test('no fatal WASM errors during the workout session', async () => {
    const fatal = await wasmApp.logOverlay.getFatalErrorLines();
    expect(fatal, `Fatal WASM errors during workout render:\n${fatal.join('\n')}`).toHaveLength(0);
  });

  // ── Retro ghost-race (RetroRace.qml + QtQuick.Shapes) ─────────────────────

  test('the retro race loads without QML errors', () => {
    expect(
      raceQmlErrors,
      `RetroRace.qml failed to load in the browser (likely a missing QML module ` +
      `such as QtQuick.Shapes in the wasm Qt build):\n${raceQmlErrors.join('\n')}`,
    ).toHaveLength(0);
  });

  test('the retro race renders (canvas is not blank)', () => {
    const stats = assertNotBlank(raceShot, 'retro race view');
    expect(stats.distinctColors).toBeGreaterThan(8);
  });

  test('showing the race changes the rendered canvas', () => {
    expect(
      raceShot.equals(workoutShot2),
      'Canvas was byte-identical before and after switching to the Game view — ' +
      'the retro race did not paint into the content pane.',
    ).toBe(false);
  });
});
