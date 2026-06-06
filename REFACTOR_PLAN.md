# Refactor / Performance / Cleanup Plan

Fresh-eyes audit (2026-06-06) of MaximumTrainer — app originally written ~2014,
migrated to Qt6 / C++17, so some parts are dated. Sequenced as **small,
independent, verifiable PRs**. Verify each finding against current code before
acting — line numbers drift as the tree changes.

Status legend: ☐ todo · ◐ in progress · ☑ done

## Progress snapshot (2026-06-06)

| Group | Status | PR |
|-------|--------|----|
| 1 — Dead-code sweep | ☑ done | #230 |
| 2 — Hot-path guards | ☑ done (2 of 4 items; 2 evaluated and intentionally skipped) | #227 |
| 3 — De-duplication via templates | ☐ todo (deferred) | — |
| 4 — Latent naming hazard | ☐ todo (deferred) | — |
| 5 — Quick correctness fixes | ☐ todo | — |
| 6 — Large mechanical modernization | ☐ todo | — |

Related work already merged/open: QWT 6.3 bump, AppImage dark-mode + OpenSSL
fixes, `QwtSystemClock` → `QElapsedTimer` (#228), `CLAUDE.md` (#229).

---

## Group 1 — Dead-code sweep  ☑
Pure deletion, zero behavioural risk. Each target verified unreferenced first.

- ☑ `fitactivitycreator.cpp` `build_FIT_file()` — dead method, never called, contained a hardcoded developer path `/Users/tourlou2/test2.fit`. Removed (+ commented `decode_FIT_file`).
- ☑ `xmlstreamwritertcx.{h,cpp}` — entirely commented out, not in any `.pri`. Removed both files + the dead `#include` in `dataworkout.cpp`.
- ☑ Course feature removed end to end (~2,200 lines): `course.*`, `coursetablemodel.*`, `sortfilterproxymodelcourse.*`, `main_coursepage.*`, `googlemapwidget.*`, plus surgical edits to util / account / settings / environnement / userdao / xmlutil / mainwindow / dialogmainwindowconfig / apptheme / z_stylesheet and the 5 integration-test `.pro` files. Kept: FIT-SDK course messages, gpxparser, the `.workout`-format "COURSE DATA" parser.

## Group 2 — Hot-path guards (perf during a workout)  ☑  (PR #227)
Built on the mini-graph throttle. Shipped the two real wins; the other two
audit items were evaluated against the actual data bounds and intentionally
skipped (risk to core data paths for no measurable gain).

- ☑ `WorkoutPlotZoomer::updateTextLabelValue` & `MinimalistWidget::setValue` — `setStyleSheet()` was called on every BLE packet (full style recompute) even when the colour was unchanged. Both now guard on the current colour, mirroring `InfoWidget::currentColor`.
- ☑ `Workout::getLstInterval()` now returns `const QList<Interval>&` instead of by value (was copying the whole interval list in several hot/loop paths). Mutating callers copy locally (COW), so safe.
- ⏭ **Skipped — rolling power running-sum** (`workoutdialog.cpp`): the averaging window is capped at **5** (combobox None/1-5 sec), so the "O(n) re-sum" is ≤5 elements. A running sum would add risk to a core averaging path for no gain.
- ⏭ **Skipped — `updateCurve` ring buffer** (`workoutplotzoomer.cpp`): the sample buffer is capped at **90** at 1-4 Hz, and Qwt's `setSamples` copies internally regardless. The original #1 audit ranking conflated this with the 40 Hz replot, which #227 already fixed.

## Group 3 — De-duplication via templates  ☐  (deferred — do later)
- ☐ `DataPower/DataHeartRate/DataCadence/DataSpeed` (+ `CurveData*` wrappers) — ~708 lines that are 99% identical → one `template<typename Tag> class DataMetric` (~150 lines).
- ☐ `PowerEditor/HrEditor/CadenceEditor` — identical NONE/FLAT/PROGRESSIVE show-hide structure → a parameterised `MetricEditor`.

## Group 4 — Latent naming hazard  ☐  (deferred — do later)
- ☐ Two classes both named `IntervalsIcuService` — `intervals_icu_service.h` (static, plain) and `intervalsicuservice.h` (QObject) — **both compiled** in `db.pri`. Links today only because their symbols happen not to collide; it's an ODR/confusion trap. Rename one (e.g. static → `IntervalsIcuApi`) or consolidate.

## Group 5 — Quick correctness fixes  ☐
- ☐ `simplecrypt.cpp:~130` — unguarded `qrand()` (removed in Qt6) → `QRandomGenerator`. (Confirm it even compiles on Qt6 CI.)
- ☐ `util.cpp:~859,~881` — `Util::zipFileToDisk`/`unzipFile` discard `QFile::open()` return → check + log + bail.
- ☐ `selfloops_service.cpp:~31` — local `gzipCompress` duplicates `Util::zipFileHelperConvertToGzip` → call the util.

## Group 6 — Large mechanical modernization (one PR per sweep; low-risk, big diff)  ☐
- ☐ 243 string-based `connect(SIGNAL/SLOT)` → function-pointer syntax (compile-time checked). Worst: `workoutdialog.cpp` (96), `mainwindow.cpp` (66).
- ☐ 45 `foreach`/`Q_FOREACH` (deprecated, removed Qt 6.7; hidden container copy) → range-`for` with `const auto&`.
- ☐ 443 `qDebug()` → the existing `LOG_*` framework, prioritising hot-path spew (`sendLoads`, `update1sec`, raw server dump at `slotGetSensorListFinished:~3012`).
- ☐ Header hygiene: `account.h` is included in 18 headers but most need only `Account*` → forward-declare; move `workoutdialog.h` include out of `dialogconfig.h`.
- ☐ `parent = 0` → `nullptr` (~60 headers); stringly-typed dispatch (`"power"/"hr"/"cad"`, `"workout"/"course"`) → enums; `typedef enum` → `enum class`.
- ☐ God-file decomposition: `workoutdialog.cpp` (4,298 lines; 590-line constructor → extract `createPairingOverlay/BatteryOverlay/AchievementOverlay/setupClockThread/setupSoundTimers`), `mainwindow.cpp` (2,629).

---

*Conventions: ship via the fork-PR flow (see `CLAUDE.md`); keep comments minimal;
prefer small PRs that build + verify locally before pushing.*
