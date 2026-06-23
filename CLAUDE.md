# CLAUDE.md — fast-start for AI agents

Operational guide for working in this repo. For deep architecture, design
patterns, and the testing strategy, read **`agents.md`** (don't duplicate it
here). For user-facing build/setup details, see **`README.md`**.

This file is the "how to actually get work done here" cheat sheet.

---

## What this is

MaximumTrainer — a cross-platform (Linux / Windows / macOS / WASM) Qt 6 / C++17
indoor cycling & rowing trainer app. BLE sensors, ERG-mode interval workouts,
FIT export, QWT plots. Build system is **qmake** (`.pro` / `.pri`), not CMake.

---

## Build & run locally (Linux)

The app builds against **QWT 6.3.0 built from source** (no Qt6 QWT apt package
exists) and points qmake at it via `QWT_INSTALL=...`.

```bash
# 1. Build QWT 6.3.0 once (if /tmp/qwt6 is gone — it's ephemeral):
cd /tmp && curl -L -o qwt.tar.bz2 \
  "https://sourceforge.net/projects/qwt/files/qwt/6.3.0/qwt-6.3.0.tar.bz2/download"
tar xf qwt.tar.bz2 && cd qwt-6.3.0
qmake6 qwt.pro && make -j$(nproc)
make install INSTALL_ROOT=/tmp/qwt6-stage
mkdir -p /tmp/qwt6 && mv /tmp/qwt6-stage/usr/local/qwt-6.3.0/* /tmp/qwt6/

# 2. Build the app (from repo root):
qmake6 MaximumTrainer.pro QWT_INSTALL=/tmp/qwt6
make -j$(nproc)

# 3. Run (QWT in a non-standard prefix needs LD_LIBRARY_PATH):
LD_LIBRARY_PATH=/tmp/qwt6/lib ./build/release/MaximumTrainer
```

- **qmake binary is `qmake6`** here (Qt 6.10 system install on Linux). CI pins Qt **6.7.3 LTS** — local Qt may be newer.
- **The `.pro` file:** **`MaximumTrainer.pro`** (renamed from `PowerVelo.pro` in PR #227). The binary `TARGET` is always `MaximumTrainer`.
- **Incremental builds:** after `qmake6`, just `make -j$(nproc)`. **A clean build needs `make clean` first** — stale `.obj` files can make `make` a silent no-op (`Nothing to be done for 'first'`) when only config changed.

### Verifying a change without a GUI session
Screenshot mode runs a real demo workout headlessly and dumps PNGs, then quits —
ideal for confirming a change renders:

```bash
LD_LIBRARY_PATH=/tmp/qwt6/lib ./build/release/MaximumTrainer --screenshots /tmp/shots
```

Note: the AppImage/release build only bundles the `xcb` platform plugin (no
`offscreen`). Locally, `xcb` works via XWayland on a Wayland session.

---

## Git / PR workflow (IMPORTANT — direct upstream access)

`maximus321` now has **write access to upstream**, so we branch and PR directly
on the main repo. **Do not use the fork** — fork PRs don't receive CI secrets
(`INTERVALS_ICU_*`, Strava), so the live integration tests would `QSKIP` instead
of actually running.

- `origin` → `MaximumTrainer/MaximumTrainer_Redux` (upstream; **push here**)
- `fork`   → `maximus321/MaximumTrainer_Redux` (legacy; **don't use for now**)

**Standard flow for any change:**
```bash
git checkout master && git fetch origin master:master   # sync upstream
git checkout -b my-feature-branch
# ... work, commit ...
git push -u origin my-feature-branch          # build.yml runs on push (with secrets)
gh pr create --repo MaximumTrainer/MaximumTrainer_Redux \
  --base master --head my-feature-branch --title "..." --body-file /tmp/body.md
```

- Pushing to an upstream branch triggers the full cross-platform `build.yml`
  immediately (`on: push: branches: ["**"]`); superseded runs auto-cancel.
- **Prune merged branches** off the shared repo: `git push origin --delete <branch>`.

**Gotchas learned the hard way:**
- **`gh pr edit` / `gh pr create` may print a GraphQL error** about "Projects
  (classic) deprecation" and silently fail the edit. Workaround: use the REST
  API — `gh api -X PATCH repos/OWNER/REPO/pulls/N --input payload.json` (build
  payload with `jq -Rs '{body: .}' body.md`). `gh pr create` via `--body-file`
  usually still works; verify with `gh pr view N --json body`.
- **Commit trailer:** end commit messages with
  `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>` (no real email).
  PR bodies end with the Claude Code generated-with line.
- **Stacked PRs:** if branch B depends on branch A's commits, prefer keeping them
  as **independent branches off master when they touch disjoint files** (they
  merge in any order, no conflict). Only stack when files genuinely overlap.

---

## Qt6 migration gotchas (this codebase specifically)

The app was migrated Qt5 → Qt6; a few traps recur:

- **`QComboBox::currentIndexChanged(const QString&)` was REMOVED in Qt6.** Any
  `connectSlotsByName` auto-slot named `on_<combo>_currentIndexChanged(QString)`
  **silently never fires.** Use an explicit `connect(...->currentTextChanged, ...)`.
  Watch the startup log for `connectSlotsByName: No matching signal` warnings —
  each is either a dead slot or a broken auto-connection.
- **`main.cpp` forces `QT_QPA_PLATFORM=xcb` under Wayland** (QtWebEngine + native
  child widgets are unstable on the Wayland QPA plugin).
- **`main.cpp` sets `QT_QPA_PLATFORMTHEME=xdgdesktopportal`** so the app picks up
  the desktop light/dark scheme under xcb (otherwise System theme → always Light).
- **`AA_ShareOpenGLContexts` is set before `QApplication`** — required or embedded
  QtWebEngine views tear down their host widget.

## AppImage packaging gotchas (Linux release)

`linuxdeploy`'s static scan misses libraries that Qt `dlopen()`s at runtime.
`release-linux.yml` therefore manually bundles:
- **NSS modules** (`libsoftokn3` & friends) — else QtWebEngine crashes on first HTTPS.
- **The OpenSSL pair** (`libssl.so.3` + `libcrypto.so.3`, matched) — a lone
  `libcrypto` causes `QSslSocket: TLS initialization failed` on hosts with a
  different OpenSSL 3.0.x point release.
- The **xdg-desktop-portal theme plugin** (for dark mode).

When adding a feature that `dlopen()`s anything, bundle it explicitly + add a
verify step.

---

## Conventions

- See `agents.md §5.4` for full coding conventions and `§4.1` for the TDD
  expectation (tests in the matching `tests/<suite>/` project).
- **Comments: minimal.** Only explain a non-obvious *why*. Do not narrate what
  standard code does, and do not annotate one-way migrations (the rationale goes
  in the commit message, not the source).
- Prefer verbose, descriptive variable names; include type info where the
  language allows.

---

## Documentation — keep it in sync (validate on EVERY PR)

There are **three** user-facing docs that drift out of sync with the code if you
let them. On any PR that changes a feature, the UI, a menu/label, a setting, a
build step, or an integration, **re-validate all three against the actual code**
(don't trust the existing prose — it has been wrong before):

- **`README.md`** — project/infra: what it is, Download (releases), build-from-source,
  testing. Keep it scoped to install/build/infra; link the User Guide for *how to use*
  the app rather than duplicating how-to content here.
- **`docs/index.html`** — the GitHub Pages **landing page** (features, gallery,
  download cards, specs, license).
- **`docs/user-guide.html`** — the GitHub Pages **User Guide** (the full how-to:
  pairing, workouts, training, Studio, uploads, settings, shortcuts).

Validation checklist:
- Cross-check every UI claim (menu paths, tab/button labels, settings categories,
  keyboard shortcuts) against the `.ui` files and slots — grep the source, don't guess.
- Cross-check build commands against `.github/workflows/build-*.yml` (Qt version,
  modules, QWT, qmake invocation). **No SFML/VLC** — media is QtMultimedia.
- Don't reference removed features (PowerCurve, Course, SelfLoops/TrainingPeaks,
  ANT+, the old web frontend) or unshipped/broken ones.
- Screenshots live in `docs/assets/screenshots/`; regenerate with
  `--screenshots /tmp/shots` (set `app_theme=1` in the QSettings conf for dark mode)
  when the UI changes. The README/guide reference them via `docs/assets/...`.
  Shortcut: run `scripts/update-screenshots.sh` — it captures the app in headless
  dark-theme mode and overwrites the docs screenshots in place (build the app first).

---

## Known dormant features (don't extend; slated for removal)

Per `agents.md §7`:
- **PowerCurve** — removed from the product. The dead UI/page was deleted in
  #242, but ~9 files still carry remnant references (`settings.h`,
  `userstudio.{h,cpp}`, `dialogconfig.{h,cpp,ui}`, `mainwindow.cpp`,
  `workoutplot.{h,cpp}`) — safe to strip when you're nearby.
- **Course** — feature fully removed end to end (Group 1 / #230: `course.*`,
  `main_coursepage.*`, `googlemapwidget.*`, etc.). Only the **FIT-SDK** course
  message headers (`src/fitness/fit/fit_course_*.hpp`) and the `.workout`
  "COURSE DATA" parser were intentionally kept. Any stray `on_action*Course*`
  slot is safe to delete.

**Removed integrations (don't re-add):** SelfLoops & TrainingPeaks uploads
(#244 — uploads now focus on Strava + Intervals.icu), the maximumtrainer.com
web frontend (#245), and ANT+ support (#240). Studio mode's old server-hosted
`QWebEngineView` is gone too (replaced by the native path in #253).

---

## In-flight work (as of 2026-06-10 — verify with `gh pr list` before relying on this)

Two big features shipped and are now in master:
- **Strava auto-upload** (#246): per-user OAuth via the **system browser**
  (loopback redirect, not an embedded webview), token exchange/refresh through a
  **Cloudflare Worker** (`workers/strava-token-proxy/`, holds the client secret),
  `Account::strava_auto_upload` toggle, and a post-workout upload panel. Worker
  is deployed and the flow is e2e-tested. Code: `strava_oauth_flow.*`,
  `strava_service.*`; tests in `tests/strava/`.
- **Studio mode** (#253): native per-rider config (**up to 15 riders**),
  per-rider sensors + FTP/LTHR, QSettings persistence, BLE + simulator support,
  JSON import/export. Replaces the dead server-hosted webview. Code:
  `studiowidget.*`, `userstudiowidget.*`, `userstudio.*`; tests in `tests/studio/`.

Other recent merges: #237 (Intervals OAuth URL fix), #238 (QLayout/pixmap/AppImage
warnings), #239 (workout-editor step-type helper), #240 (ANT remnant removal),
#241/#242 (sensor/trainer config consolidation + dead PowerCurve removal),
#243 (Intervals OAuth scopes), #244 (drop SelfLoops/TrainingPeaks), #245 (drop
web frontend), #248 (editor popup UI), #251 (audio output fix), #252 (window-on-top
+ toolbar grouping + FTP-test profile update). Earlier: #227, #228, #230, #234,
#235, #236, dark-mode + OpenSSL AppImage fixes, QWT 6.2 → 6.3.

The **workout dialog timer** is driven by a `Clock` QObject on a worker
`QThread` ticking every 25ms; it derives elapsed seconds from a monotonic
high-res clock (no drift). The three bottom mini-graphs (`WorkoutPlotZoomer`)
are throttled to ~20fps rather than replotting on every 25ms tick.
