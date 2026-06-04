# MaximumTrainer — Agents & Architecture Guide

> **Audience:** New contributors, AI coding agents, and senior reviewers.  
> **Purpose:** Define the system architecture, design patterns, cross-platform
> strategy, testing approach, and operational best-practices for the
> MaximumTrainer codebase so that every change remains consistent,
> maintainable, and safe.

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Code Architecture & Design Patterns](#2-code-architecture--design-patterns)
3. [Target Runtimes & Cross-Platform Strategy](#3-target-runtimes--cross-platform-strategy)
4. [Testing Approach & Quality Assurance](#4-testing-approach--quality-assurance)
5. [Best Practices for Maintainability & Scalability](#5-best-practices-for-maintainability--scalability)
6. [Performance & Safety](#6-performance--safety)
7. [Deferred Work & Tracking](#7-deferred-work--tracking)

---

## 1. Project Overview

MaximumTrainer is a cross-platform cycling and rowing training application
built with **Qt / C++17**.  It connects to smart trainers, power meters, heart
rate monitors, cadence sensors, and rowing ergometers via Bluetooth Low Energy
(BLE), plays structured interval workouts in ERG-mode, and exports
completed activities to the Garmin FIT format.

| Attribute | Value |
|-----------|-------|
| Language | C++17 |
| UI Toolkit | Qt 6 (Widgets + WebEngineWidgets) |
| Plotting | QWT 6.2 |
| Serialisation | Garmin FIT SDK, TCX, GPX, XML |
| Hardware protocols | BLE (Qt Bluetooth) |
| Audio/Video | QtMultimedia (video + radio), SFML (sound effects), platform stubs for WASM |
| Build system | qmake `.pro` / `.pri` |
| CI/CD | GitHub Actions (Linux · Windows · macOS · WebAssembly) |

---

## 2. Code Architecture & Design Patterns

### 2.1 Layered Architecture

MaximumTrainer is organised into **four horizontal layers**.  Each layer depends
only on the layers below it; the UI and hardware layers are isolated from each
other by the domain model.

```
┌─────────────────────────────────────────────────────────┐
│                     UI Layer                            │
│  src/ui/  (MainWindow, WorkoutDialog, WorkoutCreator)   │
│  Qt Widgets · QWT plots · .ui form files                │
└────────────────────┬────────────────────────────────────┘
                     │  Qt signals / slots
┌────────────────────▼────────────────────────────────────┐
│                  Domain / Training Engine               │
│  src/model/   (Workout, Interval, DataWorkout, …)       │
│  src/workout/ (WorkoutUtil — format conversion)         │
│  src/fitness/ (FIT SDK, Achievements)                   │
│  Pure C++ — no Qt hardware or UI dependencies           │
└───────────┬──────────────────────────┬──────────────────┘
            │                          │
┌───────────▼──────────┐  ┌────────────▼─────────────────┐
│  Hardware             │  │  Persistence                 │
│  Abstraction Layer    │  │  src/persistence/            │
│  (HAL)                │  │  SQLite DAOs (users, sensors,│
│  src/btle/            │  │  achievements), FIT/TCX/GPX  │
│  BtleHub · SimHub     │  │  file writers/readers        │
└──────────────────────┘  └──────────────────────────────┘
```

**Key invariant:** `src/model/`, `src/workout/`, and `src/fitness/` must
**never** `#include` anything from `src/btle/` or `src/ui/`.
Hardware and UI concerns are injected via Qt signals/slots and constructor
parameters.

### 2.2 Hardware Abstraction Layer (HAL)

All hardware communication goes through one of three interchangeable hub
classes that present **identical signal/slot contracts**:

| Class | Location | Purpose |
|-------|----------|---------|
| `BtleHub` | `src/btle/btle_hub.h` | Real BLE hardware via Qt Bluetooth |
| `BtleHubWasm` | `src/btle/btle_hub_wasm.h` | Web Bluetooth API via JS bridge |
| `SimulatorHub` | `src/btle/simulator_hub.h` | Synthetic data for CI & demos |

The **signal contract** shared by every hub:

```cpp
signals:
    void signal_hr     (int userID, int hr);
    void signal_cadence(int userID, int cadence);
    void signal_speed  (int userID, double speed);   // km/h
    void signal_power  (int userID, int power);      // watts
    void signal_oxygen (int userID, double smo2, double thb);

    void deviceConnected();
    void deviceDisconnected();
    void connectionError(const QString &errorString);
```

```cpp
public slots:
    void setLoad (int antID, double watts);
    void setSlope(int antID, double grade);
    void stopDecodingMsg();
```

`WorkoutDialog` connects to whichever hub is active and is unaware of the
underlying implementation.  Swapping from real hardware to simulation is
a single `connect()` call change in `MainWindow::executeWorkout()`.

### 2.3 Observer / Reactive Data Streams

High-frequency sensor data (≥10 Hz) is processed with the **Observer pattern**
using Qt's **signal/slot** mechanism:

```
BtleHub (producer)
  ──signal_power(userId, watts)──►  WorkoutDialog (consumer)
  ──signal_hr(userId, bpm)───────►  WorkoutDialog
  ──signal_cadence(userId, rpm)──►  WorkoutDialog
                                     │
                                     ▼
                              DataWorkout (recorder)
                                     │
                                     ▼
                              WorkoutPlot (renderer)
```

All signal emissions from hardware hubs happen on the **main Qt thread**.
`BtleHub` uses `QLowEnergyController` callbacks that Qt dispatches on the
thread that owns the controller object — always `MainWindow`'s thread — so no
explicit thread-safety guards are needed in the current single-threaded-UI
design.

### 2.4 Agent-Based Model

Each subsystem is modelled as an **autonomous agent** that owns its state and
communicates exclusively through Qt signals/slots:

| Agent | Owns | Communicates via |
|-------|------|-----------------|
| `BtleHub` | BLE connection state, characteristic subscriptions | Signals: `signal_*`, `deviceConnected`, `connectionError` |
| `SimulatorHub` | Timer-driven synthetic telemetry | Same signals as BtleHub |
| `WorkoutDialog` | Active workout session state (elapsed time, target power, lap index) | Receives sensor signals; emits `setLoad` / `setSlope` to hub |
| `DataWorkout` | Per-second telemetry recording | Populated by WorkoutDialog during playback |
| `AchievementChecker` | Achievement evaluation logic | Called post-workout by WorkoutDialog |
| `FitActivityCreator` | FIT file serialisation | Invoked by WorkoutDialog on session save |

This agent topology means that each component can be unit-tested in isolation
by connecting test spies instead of real counterparts.

### 2.5 Model / View Separation

All list data exposed in the UI follows Qt's **Model/View** pattern:

| Model class | Data | Used by |
|-------------|------|---------|
| `WorkoutTableModel` | `QList<Workout>` | MainWindow workout browser |
| `IntervalTableModel` | `QList<Interval>` | WorkoutCreator interval editor |
| `CourseTableModel` | `QList<Course>` | Course browser |
| `RadioTableModel` | `QList<Radio>` | Sensor configuration dialog |
| `SortFilterProxyModel` | Wraps any above | Search boxes |

Views (`QTableView`, `QListView`) are never given raw data — only model
pointers — preventing direct coupling between UI and persistence layers.

---

## 3. Target Runtimes & Cross-Platform Strategy

### 3.1 Supported Platforms

| Platform | Qt Version | Compiler | Status |
|----------|-----------|----------|--------|
| Linux (Ubuntu 22.04+) | 6.5+ | GCC 11+ | ✅ Primary development target |
| Windows 10/11 | 6.5.3 | MSVC 2019 (x64) | ✅ Release build |
| macOS 13+ (Apple Silicon / Intel) | 6.5.3 | Clang | ✅ Release build |
| WebAssembly (browser) | 6.6.3 | Emscripten 3.1.43 | ⚠️ Best-effort (`continue-on-error`) |

### 3.2 Shared Core, Platform Adapters

The strategy is **"maximise shared C++ core; isolate platform differences
behind compile-time guards or swappable adapters"**.

```
┌────────────────────────────────────┐
│         Shared Core (all targets)  │
│   model/ · workout/ · fitness/     │
│   persistence/ · ui/               │
└──────────┬─────────────────────────┘
           │
     ┌─────▼──────────────────────────────────────────┐
     │         Platform adapters (selected at build)   │
     │                                                 │
     │  BLE:     btle_hub.cpp         (desktop)        │
     │           btle_hub_wasm.cpp    (WASM)           │
     │           webbluetooth_bridge  (WASM JS bridge) │
     │                                                 │
     │  Media:   myqtmediaplayer.cpp  (QtMultimedia)   │
     │           soundplayer.cpp      (SFML desktop)   │
     │           soundplayer_wasm.cpp (WASM stub)      │
     │                                                 │
     │  Scanner: btle_scanner_dialog.cpp   (desktop)   │
     │           btle_scanner_dialog_wasm  (WASM)      │
     │                                                 │
     │  WebEngine: real QtWebEngineWidgets (desktop)   │
     │             src/ui/wasm_stubs/ header stubs     │
     └─────────────────────────────────────────────────┘
```

Platform selection is controlled **at qmake time** using `.pro` / `.pri`
scopes (`wasm`, `win32`, `macx`, `linux`) and optional defines
(`GC_HAVE_QTMULTIMEDIA`).  No runtime `#ifdef` branching inside shared logic files.

### 3.3 WASM-Specific Constraints

| Concern | Constraint | Mitigation |
|---------|-----------|-----------|
| No native BLE APIs | Web Bluetooth is promise-based | `BtleHubWasm` + `WebBluetoothBridge` (Emscripten embind + JS callbacks) |
| No file-system access | `QFile` writes are in-memory | Persist via browser download prompt |
| No QtMultimedia / SFML | Shared libs unavailable | `soundplayer_wasm.cpp` stub; no video |
| Single-threaded Emscripten | `pthread` unavailable (singlethread build) | No `QThread` use in WASM paths |
| `QWebEngineWidgets` absent | Not ported to WASM | Stub headers in `src/ui/wasm_stubs/` |

### 3.4 Bluetooth LE Architecture

```
Desktop (Qt Bluetooth)              WASM (Web Bluetooth)
─────────────────────               ──────────────────────
BtleHub                             BtleHubWasm
  └─ QLowEnergyController             └─ WebBluetoothBridge (C++)
       └─ Qt platform plugin               └─ JavaScript embind bindings
            └─ OS BLE stack                     └─ navigator.bluetooth API
```

Both paths emit **identical signals** to `WorkoutDialog`, ensuring zero
divergence in training-engine logic between platforms.

---

## 4. Testing Approach & Quality Assurance

### 4.1 Test-Driven Development (TDD) — Red · Green · Refactor

**Every change to this codebase must begin with a failing test.**
Follow the classic red-green-refactor cycle for all bug fixes and feature work:

```
┌─────────────────────────────────────────────────────────────┐
│  RED    Write a test that describes the desired behaviour.  │
│         Run it — confirm it FAILS for the right reason.     │
│         (A test that passes immediately proves nothing.)    │
├─────────────────────────────────────────────────────────────┤
│  GREEN  Write the minimum production code to make the test  │
│         pass.  Do not gold-plate at this stage.             │
├─────────────────────────────────────────────────────────────┤
│  REFACTOR  Clean up — naming, duplication, structure.       │
│            All tests must still be GREEN after refactoring. │
└─────────────────────────────────────────────────────────────┘
```

**Practical rules for this codebase:**

1. **Add the test first.** Before touching `src/`, add a test case (or extend
   an existing one) in the appropriate `tests/` project.  The CI pipeline must
   show it failing before the fix lands.
2. **One failing test per change.** A single test failure pinpoints the
   missing behaviour.  Large test additions added all at once are a red flag
   that TDD was skipped.
3. **Use `simulateNotification()` for BLE changes.** New BLE characteristic
   parsers must have a `testXxx_*` case in `tests/btle/tst_btle_hub.cpp`
   that demonstrates the failure mode first.
4. **Use `SimulatorHub` for training-engine changes.** Integration tests in
   `tests/integration/` drive `WorkoutDialog` via `SimulatorHub`; write the
   interaction scenario in the test before implementing it.
5. **Use Playwright for UI/WASM changes.** Any visible change to the WASM
   app or landing page requires a Playwright spec addition (`.spec.ts`) that
   fails against the current deployment before the change ships.
6. **CI enforces the rule.** All test jobs run on every push.  A PR is not
   mergeable if any job that was previously passing starts failing due to
   unrelated test removals or skips.

### 4.2 Testing Pyramid

```
              ╔══════════════════╗
              ║  System / E2E    ║  (Playwright — WASM, landing page, BLE API)
              ╚══════╤═══════════╝
           ╔═════════╧════════════════╗
           ║  Integration Tests       ║  (WorkoutDialog + SimulatorHub, login,
           ╚═══════╤══════════════════╝   offline/online mode, BLE adapter API)
        ╔══════════╧═══════════════════════╗
        ║  Unit Tests                      ║  (BLE parsing, model, services,
        ╚══════════════════════════════════╝   logger, credential store, …)
```

### 4.3 Unit Tests — `tests/btle/`

**Project file:** `tests/btle/btle_tests.pro`  
**Runner:** `tests/btle/tst_btle_hub.cpp` (51 test cases)  
**Framework:** Qt Test (`QTest`, `QSignalSpy`)

Tests exercise `BtleHub::simulateNotification()` — a dedicated test hook that
injects raw BLE characteristic byte arrays as if received from hardware:

```cpp
// Inject a Heart Rate Measurement characteristic notification
hub.simulateNotification(0x2A37, QByteArray::fromHex("0060"));   // 96 bpm
QCOMPARE(spy.count(), 1);
QCOMPARE(spy.takeFirst().at(1).toInt(), 96);
```

**Coverage areas:**

| Group | Tests | What is verified |
|-------|-------|-----------------|
| HR parsing | 6 | 8-bit/16-bit flags, RR-interval presence, zero, max, too-short guard |
| CSC parsing | 7 | Crank-only, wheel-only, combined, uint16 rollover, standstill, first-measurement discard, too-short guard |
| Power parsing | 4 | Positive, zero, negative (track-stand), too-short guard |
| FTMS parsing | 8 | Speed-only, cadence-only, power-only, all-fields, zero values, negative power, optional-fields skip, too-short guard |
| Trainer sims | 5 | Elite single packet, Elite sequence, Wahoo KICKR (power + CSC), Wahoo CSC rollover, Garmin Tacx (FTMS + CSC) |
| SimulatorHub | 8 | Signal emission (hr/cadence/speed/power), drift-within-bounds, stop suppresses signals, no-op setLoad/setSlope |
| Battery | 5 | Above/at/below threshold emission, too-short guard, clamping |
| Interval summary | 8 | Met (exact/upper/lower boundary), near-miss (upper/lower), missed (above/below), zero-target |

**Build & run (from repository root):**

```bash
cd tests/btle
qmake btle_tests.pro && make -j$(nproc)
cd ../..
./build/tests/btle_tests -v2
```

### 4.4 Service & Model Unit Tests

Each service and domain component has its own standalone Qt Test project that
requires no display or hardware:

| Test project | Location | What is tested |
|-------------|----------|----------------|
| `intervals_icu_tests.pro` | `tests/intervals_icu/` | `IntervalsIcuService` — auth headers, URL construction, query params, null-manager guard |
| `importer_workout_zwo_tests.pro` | `tests/intervals_icu/` | ZWO XML parser — SteadyState, Ramp, IntervalsT, FreeRide, mixed, malformed input |
| `intervals_icu_dao_bearer_tests.pro` | `tests/intervals_icu/` | OAuth2 Bearer-token DAO methods |
| `strava_tests.pro` | `tests/strava/` | Strava upload/status/deauthorize headers, URLs, null-manager guard |
| `trainingpeaks_tests.pro` | `tests/trainingpeaks/` | TrainingPeaks upload/refresh headers, URLs, no hardcoded secret |
| `selfloops_tests.pro` | `tests/selfloops/` | Selfloops upload URL construction, POST method, null-manager guard |
| `credential_store_tests.pro` | `tests/credential_store/` | Round-trip read/write, overwrite, remove, missing key, multi-service, WASM no-op |
| `plan_adherence_tests.pro` | `tests/plan_adherence/` | Completed/skipped/substituted entries, adherence %, encode-decode round-trip, change signals |
| `logger_tests.pro` | `tests/logger/` | Log level filtering, output format (timestamp/level/module/message), file write enable/disable |
| `studio_tests.pro` | `tests/studio/` | Multi-hub simultaneous signals, user-ID propagation, FTP scaling formula, settings persistence |

**Build & run pattern (same for every service test):**

```bash
cd tests/<suite>
qmake <suite>_tests.pro && make -j$(nproc)
cd ../..
./build/tests/<suite>_tests -v2
```

### 4.5 Integration Tests — `tests/integration/`

All integration test projects in `tests/integration/` require Qt Widgets and
run under a virtual display (`Xvfb`) in CI.  Each project produces a
screenshot artifact that is uploaded to the CI run for visual inspection.

| Project | Test(s) | What is covered |
|---------|---------|-----------------|
| `btle_integration_tests.pro` | `testBtleActivityRunning` | Full workout session driven by `SimulatorHub` through `WorkoutDialog` into `DataWorkout`; ERG-mode load commands; lap transitions |
| `btle_api_tests.pro` | `testBleAdapterQuery`, `testBtleHubSmokeTest` | BLE adapter presence query; `BtleHub` smoke test without physical hardware |
| `runtime_validation_tests.pro` | `testRuntimeValidation` | Qt version, BLE availability, database connectivity, FIT SDK version |
| `offline_mode_tests.pro` | `testLocalWorkoutAccess`, `testBtleSimulatorCyclingData`, `testOfflineModeScreenshot` | Local workout XML access; simulator cycling data; full offline-mode 1280×720 screenshot |
| `login_screen_tests.pro` | `testOfflineLogin`, `testIntervalsIcuOAuthUrlGeneration`, `testIntervalsIcuApiLogin`, `testDialogLoginInitialState`, `testDialogLoginOfflineFlow`, `testDialogLoginIntervalsIcuButton`, `testDialogLoginIntervalsIcuOAuthDialog` | Login dialog state machine; offline login path; OAuth2 URL generation; Intervals.icu API login |
| `online_mode_tests.pro` | `testOnlineModeAuthentication`, `testCalendar`, `testWorkoutPush`, `testWorkoutPull` | Live Intervals.icu API authentication, calendar fetch, workout push/pull (skipped when secrets absent) |
| `workout_ui_tests.pro` | 15 test cases (XML create/retrieve, ERG load/slope, session lifecycle, interval advancement, data accumulation, screenshot, model construction, round-trip, average power, network connectivity/retrieval, power-on-target) | Complete workout session UI driven by `SimulatorHub`; screenshot at execution |

**Run integration tests (from repository root, after build):**

```bash
Xvfb :99 -screen 0 1920x1080x24 &
export DISPLAY=:99
./build/tests/btle_integration_tests -v2
./build/tests/workout_ui_tests -v2
# … etc.
```

**Intervals.icu integration (live network):**

```bash
cd tests/intervals_icu_integration
qmake intervals_icu_integration_tests.pro && make -j$(nproc)
cd ../..
./build/tests/intervals_icu_integration_tests -v2
```

### 4.6 Hardware Mocks & Stubs

| Mechanism | File | Used in |
|-----------|------|---------|
| `SimulatorHub` | `src/btle/simulator_hub.cpp` | UI simulation mode, integration tests |
| `BtleDeviceSimulator` | `tests/btle/btle_device_simulator.h` | Unit test byte-level fake device |
| WASM audio stub | `src/app/soundplayer_wasm.cpp` | WASM build (no SFML) |
| WASM WebEngine stub | `src/ui/wasm_stubs/` | WASM build (no QtWebEngineWidgets) |

The design principle is: **no test should require physical hardware**.
`SimulatorHub` can replace any real hub at the `MainWindow` level.
`BtleHub::simulateNotification()` covers byte-level parsing without a BLE
adapter.

### 4.7 WASM / Browser Tests — `tests/playwright/`

**Config:** `playwright.config.ts` (root)  
**Framework:** Playwright (TypeScript, Chromium-first)  
**Page Object Model:** `tests/playwright/pages/` (`WasmAppPage`, `LandingPage`) and `tests/playwright/widgets/`

Playwright tests run against the live GitHub Pages deployment
(`https://maximumtrainer.github.io/MaximumTrainer_Redux/app/`).

| Spec file | What is tested |
|-----------|----------------|
| `wasm_webapp.spec.ts` | WASM asset availability (qtloader.js, MaximumTrainer.js/.wasm, logger.js); page load; log overlay; BLE mock GATT flow; browser-compatibility warnings; PWA manifest; reconnect overlay |
| `landing_page.spec.ts` | Landing page assets, title, hero section, navigation links, features/download sections, console errors |
| `wasm_btle_api.spec.ts` | Web Bluetooth API surface — scan, connect, GATT characteristic interactions via injected mock |
| `wasm_login_verification.spec.ts` | Login screen display and Intervals.icu OAuth redirect from the WASM app |
| `wasm_intervals_icu_functional.spec.ts` | Intervals.icu workout import flow end-to-end in the WASM app |
| `intervals_icu.spec.ts` | Intervals.icu landing page integration |

A `navigator.bluetooth` stub is injected via `addInitScript()` so the app
does not abort on browsers without real BLE.  The WASM binary is loaded once
per `test.describe` block using `test.beforeAll` with a 300 s timeout to
absorb JIT-compile time on cold CI runners.

**Run locally (after WASM deployment):**

```bash
npx playwright install chromium
npx playwright test
```

### 4.8 CI/CD Pipeline

```
Push to branch
      │
      ├─► build_linux ──► test_btle_unit              (51 unit tests)
      │                ├─► test_btle_integration       (Xvfb, screenshot)
      │                ├─► test_btle_api               (BLE adapter smoke)
      │                ├─► test_runtime_validation     (Qt/BLE/DB/FIT checks)
      │                ├─► test_offline_mode           (Xvfb, screenshot)
      │                ├─► test_login_screen           (Xvfb, screenshot)
      │                ├─► test_workout_ui             (Xvfb, screenshot)
      │                ├─► test_intervals_icu          (service + ZWO parser)
      │                ├─► test_intervals_icu_integration (live network, skipped without secrets)
      │                ├─► test_online_mode            (live network, skipped without secrets)
      │                ├─► test_credential_store
      │                ├─► test_plan_adherence
      │                └─► test_logger
      ├─► build_windows
      ├─► build_mac
      └─► build_wasm (continue-on-error)
              │
      (master only, all non-wasm pass)
              │
              ▼
        tag_release (auto-increment semver, dispatch release.yml)
              │
              ▼
        release.yml: create GitHub Release + attach artefacts
              │
              ▼
        pages.yml: deploy docs/ + WASM to GitHub Pages
              │
              ▼
        test_playwright (Chromium headless, 6 spec files)
```

All build jobs run in parallel.  WASM failure does not block release
publication (`continue-on-error: true` + `always()` guard on publish job).
Test jobs that require live network credentials (`test_online_mode`,
`test_intervals_icu_integration`) call `QSKIP` when secrets are absent, so
they degrade gracefully on fork PRs.

---

## 5. Best Practices for Maintainability & Scalability

### 5.1 Dependency Injection

- **Hub injection:** `WorkoutDialog` receives its hub via `connect()` calls
  in `MainWindow::executeWorkout()`.  It never instantiates `BtleHub` or
  `SimulatorHub` directly.
- **DAO injection:** All database access objects (`UserDAO`, `SensorDAO`, …)
  are constructed once in `Environnement` (note: French spelling — the actual
  class name in `src/persistence/db/environnement.h`) and passed to consumers.
  No static/singleton DAO access.
- **Settings injection:** `Account` and `Settings` objects are passed into
  dialogs via constructor parameters or `setAccount()` setters, not obtained
  from global state inside the dialog.
- **Test guideline:** Any class that cannot be unit-tested by replacing its
  hardware or database dependency with a stub/mock is a dependency-injection
  violation.

### 5.2 Error Handling Boundaries

| Boundary | Mechanism | Recovery |
|----------|-----------|---------|
| BLE connection loss | `BtleHub::connectionError(QString)` signal | `WorkoutDialog` shows reconnect dialog; pauses ERG commands |
| BLE reconnect | `BtleHub` internal `QTimer` (5 s) re-invokes `connectToDevice` | Up to 3 automatic retries |
| File I/O failure | Return `false` + `qWarning()` in all `XmlUtil`, `FitActivityCreator` methods | UI shows save-failure dialog |
| Database error | `QSqlQuery::lastError()` checked after every exec; logged | Operation skipped; no crash |
| WASM asset load failure | `qtloader.js` error callback sets `#loading-screen` error text | User sees graceful "load failed" page |

**Rule:** No hardware or I/O error should propagate as an unhandled exception
or cause an undefined-behaviour crash.  Errors are logged with `qWarning()` /
`qCritical()` and surfaced to the user via a status label or dialog.

### 5.3 Modularisation

The codebase is segmented into **qmake `.pri` modules**, each with a single
responsibility:

| Module (`.pri`) | Responsibility | Internal dependencies |
|-----------------|----------------|----------------------|
| `src/model/model.pri` | Pure domain model (Workout, Interval, Course, …) | None |
| `src/workout/workout.pri` | Workout file conversion utilities | `model` |
| `src/btle/btle.pri` | BLE HAL (hub + scanner + simulator) | `model` |
| `src/persistence/persistence.pri` | SQLite DAOs + file readers/writers | `model`, `fitness` |
| `src/fitness/fitness.pri` | FIT SDK + Achievement logic | `model` |
| `src/ui/ui.pri` | All UI: MainWindow, WorkoutDialog, plots, editors | All above |
| `src/app/app.pri` | Entry point + global state (media, audio, utils) | `ui`, `btle`, `model` |

**Adding a new feature (TDD order):**

1. Write a failing test in the appropriate `tests/` project (red).
2. Define domain types in `model/`.
3. Add persistence in `persistence/`.
4. Add business logic in `workout/` or `fitness/`.
5. Wire UI in `ui/`.
6. All tests must be green at each layer before proceeding to the next.
7. Never skip layers.

### 5.4 Coding Conventions

- **C++17** throughout (`std::optional`, structured bindings, `if constexpr`).
- Include paths are resolved by `INCLUDEPATH` in `.pro` — use bare filenames
  without path prefixes in `#include` directives for files within the same
  module (e.g., `#include "btle_hub.h"`).  For cross-module includes or
  third-party headers, prefer the shortest unambiguous path
  (e.g., `#include "../../src/btle/btle_hub.h"` in test files that live
  outside `src/`).
- UI files (`.ui`) are machine-edited by Qt Designer; hand-edit only for
  property additions that Designer cannot express.
- `Q_OBJECT` is required on every class that uses signals or slots.
- Signal parameter types must be **value types or `const &`** — never raw
  pointers — to avoid lifetime issues across thread boundaries.
- Use `qint64` / `quint16` etc. (not `long` / `unsigned short`) for any value
  that maps to a BLE/FIT wire type.

---

## 6. Performance & Safety

### 6.1 Memory Management for Long-Duration Workouts

`DataWorkout` accumulates one `TrackPoint` per second.  A 2-hour workout
generates ~7 200 points.  At ~64 bytes each, peak heap usage for telemetry
is **< 500 KB** — well within any platform's capacity.

Guidelines:
- **Do not** store raw BLE notification byte arrays long-term; parse
  immediately in `onCharacteristicChanged()` and discard.
- `WorkoutPlot` curves are backed by `QwtSeriesData` that references the same
  `DataWorkout` vector — no copies.  Append-only; never remove mid-workout.
- `FitActivityCreator` writes the FIT file in a **single pass** at session end.
  It does not buffer the entire file in memory; records are encoded and
  written incrementally.
- After session save, `DataWorkout` is owned by the history model and not
  retained by `WorkoutDialog` (it is passed via `std::move` or pointer
  transfer to the history-list owner).

### 6.2 Thread Safety for High-Frequency Sensor Data

MaximumTrainer uses a **single-threaded event loop** design:

- All BLE callbacks (`QLowEnergyController`, `QLowEnergyService`) are delivered
  on the thread that owns the controller — the main thread.
- `SimulatorHub` fires a `QTimer` on the main thread.
- `WorkoutDialog` processes signals on the main thread.
- **No shared mutable state is accessed from multiple threads**, so no mutex
  locking is needed in the hot path.

If a future change introduces a worker thread (e.g., for FIT file export):

- Use `QThread` + `QObject::moveToThread()` — never subclass `QThread`.
- All cross-thread communication must use **queued signal/slot connections**
  (automatic when objects live on different threads in Qt).
- Any shared data structure updated from a worker thread must be protected by
  `QMutex` / `QReadWriteLock`.
- **Never** call `QWidget` methods from a non-main thread.

### 6.3 BLE Reconnection & Stability

`BtleHub` implements automatic reconnection:

```
onControllerDisconnected()
        │
        ▼ start m_reconnectTimer (5 s)
        │
        ▼ connectToDevice() [re-entry, up to 3 attempts]
        │
   success ──► serviceDiscoveryFinished() ──► re-subscribe notifications
   failure ──► emit connectionError(...)   ──► WorkoutDialog informs user
```

- BLE operations that may stall emit `connectionError` after a configurable
  timeout (`QTimer` guard on `onDiscoveryFinished`).
- ERG commands (`setLoad` / `setSlope`) are **fire-and-forget**; they do not
  block.  If the device is disconnected, the command is silently dropped and
  the reconnect flow is already in progress.

### 6.4 WASM Single-Thread Constraints

- All WASM paths are compiled with Emscripten's **single-threaded** runtime
  (`Qt WASM singlethread`).
- No `QThread`, no `std::thread`, no POSIX threads.
- Asynchronous BLE operations (Web Bluetooth) are handled via
  `emscripten::val` callbacks marshalled back to the Qt event loop through
  `WebBluetoothBridge`.
- Large synchronous operations (e.g., FIT file creation) must be kept
  **< 16 ms** per invocation to avoid blocking the browser's main thread.
  If ever they exceed this, split with `QTimer::singleShot(0, …)` to yield.

---

## 7. Deferred Work & Tracking

Known follow-up work and intentional technical decisions that are not obvious
from the code alone.

### 7.1 Deferred Tasks

**Remove the PowerCurve feature entirely.** PowerCurve was removed from the
product but still exists throughout the code (~20 files, 100+ references).
Scrape it out as its own dedicated PR (large; touches plotting, studio users,
and persistence):

- `src/model/powercurve.{cpp,h}` (delete)
- References across `account.{cpp,h}`, `userstudio.{cpp,h}`,
  `workoutplot.{cpp,h}`, `mainwindow.{cpp,h}`, `dialogconfig.{cpp,h}`,
  `workoutdialog.{cpp,h}`, `util.cpp`, `globalvars.cpp`, `zoneobject.cpp`,
  `xmlutil.cpp`, `userdao.cpp`, `settings.h`
- While at it, remove the orphaned profile-physio fields listed in §7.2.

**Offline achievement tracking.** Achievements were a sub-tab of the
(now-removed) main-page Profile tab, rendered by a server-hosted
`QWebEngineView` (`webView_achiev` → `Environnement::getUrlAchievement()`), and
were dropped from the UI. To bring them back offline: compute unlocks locally
(`src/fitness/achievements/achievementchecker.*` and `managerachievement.*`
already exist but currently round-trip through `AchievementDao` network calls),
add local persistence for unlocked achievements, and build a native (non-web)
achievements UI.

### 7.2 Settings Persistence — Local Only

The `maximumtrainer.com` account endpoint that historically stored user
settings is **defunct**. All user-editable settings are persisted locally in
the `displayPrefs` QSettings group via `Account::save/loadDisplayPrefs()` (with
encrypted credential stores for third-party tokens). **When adding a new user
setting, persist it there — do not rely on the server.**

**Profile physio fields are intentionally NOT persisted.** These `Account`
fields only ever came from the server JSON and have no remaining UI, so
persisting them would just store hardcoded defaults. They are left unpersisted
and should be **removed with the PowerCurve cleanup** (§7.1):
`height_cm`, `bike_weight_kg`, `wheel_circ`, `bike_type`, `minutes_rode`,
`powerCurve`.

(`FTP`, `LTHR`, and `weight_kg` *are* persisted — still user-editable via
**Preferences → Profile**.)

---

*This document should be updated alongside any architectural change.
When adding a new hardware protocol, runtime target, or test tier, update the
relevant section and add an entry to the layer diagram.*

<!-- ci: validate concurrency auto-cancel -->
