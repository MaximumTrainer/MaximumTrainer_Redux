# Studio Mode — status & remaining work

Studio mode lets several riders train at once, each with their own sensors and
metrics. It was originally driven by a **server-hosted `QWebEngineView`**
(`webView_studio`, `getUrlStudio()`) that no longer loads, so the whole feature
was effectively dead. We are rebuilding it natively.

Status legend: ☑ done · ◐ partial · ☐ todo

---

## Done

- ☑ **Native `StudioWidget`** (`src/ui/studiowidget.{h,cpp}`) replaces the dead
  `webView_studio` on the main-window **Studio** tab. Exposes **Enable Studio
  Mode** + **Number of riders**.
- ☑ Wired to the real side-effects: `MainWindow::enableStudioMode` (window title,
  disables the History tab) and `MainWindow::setNumberUserStudio`.
- ☑ Removed the dead studio web machinery: `webView_studio` + its `.ui` page,
  `fillStudioPage`, `companyLoadedForUser`, the studio `QWebChannel`, the studio
  branch of `sendDataToSettingsOrStudioPage`, and the orphaned
  `set/disablePowerCurveForUser` JS-bridge slots.

## Remaining for studio mode to actually work end-to-end

- ☐ **Per-rider configuration UI** in `StudioWidget`: for each active rider
  (1…N), a compact row with display name, FTP/LTHR, and **sensor selection**
  (HR / Power / Cadence-Speed / Trainer), styled like the Bluetooth Sensors page
  but smaller so N rows fit. Show exactly `nb_user_studio` rows.
- ☐ **Per-rider sensor storage.** `BtleSensorStore` currently saves ONE device
  per role (single rider). Needs a per-rider dimension (e.g. key by rider index)
  or a parallel studio store.
- ☐ **Local persistence** of `enable_studio_mode`, `nb_user_studio`, and the
  per-rider config. These were server-only (now-defunct) — nothing saves them to
  QSettings today, so they reset on restart. (See `Account` save methods for the
  pattern, e.g. `saveDisplayPrefs`.)
- ☐ **Populate `vecUserStudio` natively.** `WorkoutDialog` studio mode reads
  `vecUserStudio` (per-rider `UserStudio`: name, FTP, sensor IDs). The web page
  used to fill it; `StudioWidget` must now build it from the native config.
- ☐ **Multi-rider BLE in the workout path.** `MainWindow::startWorkoutWithHubs`
  is single-rider (`BtleHub::SOLO_USER_ID = 1`, one hub-set). Studio mode needs
  to connect a sensor set **per rider** and route each rider's signals to
  `arrDataWorkout[i]` / `arrUserStudioWidget[i]` (userID = rider index, 1-based).
- ☐ **Retire leftover studio JS-bridge.** `updateFieldForUser` (and friends) are
  now unreachable (the WebChannel is gone) — remove once the native path lands.

## Notes / gotchas

- `arrDataWorkout[userID-1]` / `arrUserStudioWidget[userID-1]` are indexed by a
  **1-based** rider id; id 0 underflows (see the SimulatorHub `setUserID(1)`
  comment in the screenshot harness).
- Rider count was historically capped at **6** (old combobox); keep 6 unless we
  decide otherwise.
- The `UserStudio` model still carries vestigial `usingPowerCurve` / company /
  brand fields from the removed trainer-curve feature — ignore or clean up.
