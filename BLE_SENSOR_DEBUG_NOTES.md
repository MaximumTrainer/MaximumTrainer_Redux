# BLE sensor debug / simulation — design notes & findings

Goal: exercise every BLE sensor type individually against the real data
pipeline and confirm the UI reflects it, so silent breakages (like the ERG /
trainer-control bug, where `trainerControlUserId` stayed -1 and `sendLoads()`
no-op'd) are caught.

## Data pipeline (desktop)
Hub (BtleHub, or SimulatorHub stand-in) → Qt signals → WorkoutDialog slots → widgets.

Hub → WorkoutDialog signal/slot wiring (`mainwindow.cpp`):
- `signal_hr(uid,int)`        → `HrDataReceived`        → `wid_1_infoBoxHr` / HR graph
- `signal_cadence(uid,int)`   → `CadenceDataReceived`   → `wid_3_infoBoxCadence`
- `signal_speed(uid,double)`  → `TrainerSpeedDataReceived` → `wid_4_infoBoxSpeed` / distance
- `signal_power(uid,int)`     → `PowerDataReceived`     → `wid_2_infoBoxPower`
- `signal_oxygen(uid,d,d)`    → `OxygenValueChanged`    → `wid_oxygen`

Trainer control (outbound): WorkoutDialog emits `setLoad(antId,watts)` /
`setSlope(antId,grade)` → hub. Guarded by `trainerControlUserId` (−1 = OFF).
`enableTrainerControl()` sets it to 1; if never called, ERG silently does nothing.
ERG smoothing ramps via `setLoad` (see startErgSmoothing/ergSmoothStep).

## CONFIRMED FINDINGS (code audit)
- 🔴 **L/R power balance + pedal metrics are non-functional end-to-end.**
  - `parsePowerMeasurement` (0x2A63, btle_hub.cpp:695) reads ONLY the power
    bytes and ignores the flags — including Pedal Power Balance (flag bit 0).
  - No `signal_balance`/`signal_pedal` exists on the hub; nothing carries it.
  - `PowerBalanceDataReceived` / `pedalMetricReceived` have ZERO connections —
    dead slots. So `wid_2_balancePower` + pedal torque/smoothness never update.
  - This is a real bug and a sensor type Maxime explicitly wants tested.
- 🟢 **ERG / trainer control is wired**: `enableTrainerControl()` is called in
  all three workout-launch paths (mainwindow.cpp 1360/1515/1627), so
  `trainerControlUserId` = 1 and `sendLoads()` emits `setLoad`. The original
  "stuck at −1" bug is fixed — the harness should regression-guard it.
- 🟢 HR / power / cadence / speed / oxygen each have a decoder + hub signal +
  WorkoutDialog slot + widget (to be empirically confirmed by the harness).

## (original) suspected gaps to verify
1. **L/R power balance + pedal metrics appear UNWIRED.** `BtleHub` exposes no
   balance/pedal signal, and nothing in `mainwindow.cpp` connects
   `PowerBalanceDataReceived` / `pedalMetricReceived`. So `wid_2_balancePower`
   and pedal torque/smoothness may never update from a real sensor. VERIFY:
   does any decoder emit these? If not, that's a real bug.
2. **ERG / trainer control**: verify the workout actually emits `setLoad` with
   the right watts in ERG mode (i.e. `trainerControlUserId` got set). This is
   the regression class that motivated this task.

## RESULTS (--sensor-check, headless, all widgets enabled)
Run: `./MaximumTrainer --sensor-check <dir>` → per-sensor screenshots + report.

| Sensor | Result |
|---|---|
| HR        | ✅ 152 bpm shown (top bar + HR widget) |
| Power     | ✅ 255 W shown |
| Cadence   | ✅ 92 rpm shown |
| Speed     | ✅ 34.5 km/h (Trainer Speed) shown |
| Oxygen    | ✅ 62% SmO2 / 12.5 tHb shown (when the oxygen widget is enabled) |
| L/R balance + pedal | ❌ broken end-to-end (no decode, no signal, dead slots; no balance widget surfaces even when enabled + slot called directly) |
| ERG / trainer control | ✅ 6× `setLoad ant=1 126 W` captured = warm-up target ≈0.6×FTP — ERG drives the trainer |

Net: the core sensors + ERG work; **L/R balance + pedal metrics are the one broken sensor type.**

## Suggested fix for balance/pedal (for review — NOT yet implemented)
1. `parsePowerMeasurement` (0x2A63): read flags; if bit 0 (Pedal Power Balance
   Present), parse the balance byte (uint8, 0.5%, reference = left). Optionally
   parse torque-effectiveness / pedal-smoothness optional fields.
2. Add `signal_balance(uid,int rightPct)` (and a pedal-metrics signal) to BtleHub
   and emit from the decoder.
3. Connect them to `PowerBalanceDataReceived` / `pedalMetricReceived` in the
   workout-launch paths (mainwindow.cpp), like the other sensor signals.
4. Ensure `wid_2_balancePower` is actually placed in the metric band (it isn't
   surfaced today even with `show_power_balance_widget` = true — likely needs to
   be in `account->tab_display` / moveWidgetsPosition).
This touches the decoder + hub + wiring + UI layout, so it's left for review.

## Harness plan
Add a headless validation pass (mirrors `--screenshots`): drive a demo workout
with a controllable `SimulatorHub`, in ERG, then per sensor type inject a known
value, `processEvents`, and grab a screenshot + log the widget value. Capture
emitted `setLoad`/`setSlope` to prove trainer control fires. Sensor types to
cover: HR, power, cadence, speed, oxygen, L/R balance, pedal metrics, + ERG
load output. Output: one screenshot per type + a pass/fail report.

Extend `SimulatorHub` (or a SensorSimulator) to ALSO emit balance + pedal
metrics so those paths can be exercised once wired.
