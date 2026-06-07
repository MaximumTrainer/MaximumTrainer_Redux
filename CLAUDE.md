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

## Git / PR workflow (IMPORTANT — fork-based)

This repo has **two remotes**:
- `origin` → `MaximumTrainer/MaximumTrainer_Redux` (upstream; **no push access**)
- `fork`   → `maximus321/MaximumTrainer_Redux` (push here)

**Standard flow for any change:**
```bash
git checkout master && git fetch origin master:master   # sync upstream
git checkout -b my-feature-branch
# ... work, commit ...
git push -u fork my-feature-branch
gh pr create --repo MaximumTrainer/MaximumTrainer_Redux \
  --base master --head maximus321:my-feature-branch --title "..." --body-file /tmp/body.md
```

**Gotchas learned the hard way:**
- **The fork's `master` drifts.** A `github-actions[bot]` workflow runs on the
  fork and periodically commits stale "deploy Wasm artifacts for vX.Y.Z" commits
  straight to `fork/master`. Before syncing, expect to discard these (force-push
  upstream's master over them: `git push fork master:master --force-with-lease=...`).
  Consider disabling `build.yml`/`release.yml`/`pages.yml` on the fork to stop it.
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

## Known dormant features (don't extend; slated for removal)

Per `agents.md §7`:
- **PowerCurve** — removed from the product, ~20 files of dead references remain.
- **Course** — fully dormant (`main_coursepage` commented out of `ui.pri`).
  When you see orphaned `on_action*Course*` slots, they're safe to delete.

---

## In-flight work (as of 2026-06-07 — verify with `gh pr list` before relying on this)

Recently merged: #227 (Qt6 combobox fixes + `MaximumTrainer.pro` rename + mini-graph throttle),
#228 (`QwtSystemClock` → `QElapsedTimer`), #230 (dead-code sweep), #234 (zip error handling),
#235 (`IntervalsIcuService` → `IntervalsIcuApi` rename), #236 (DataMetric CRTP template),
#237 (Intervals OAuth URL fix), dark-mode + OpenSSL AppImage fixes, QWT 6.2 → 6.3 bump.

The **workout dialog timer** is driven by a `Clock` QObject on a worker
`QThread` ticking every 25ms; it derives elapsed seconds from a monotonic
high-res clock (no drift). The three bottom mini-graphs (`WorkoutPlotZoomer`)
are throttled to ~20fps rather than replotting on every 25ms tick.
