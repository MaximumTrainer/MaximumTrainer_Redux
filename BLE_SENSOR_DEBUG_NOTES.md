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

## FINDINGS / suspected gaps to verify
1. **L/R power balance + pedal metrics appear UNWIRED.** `BtleHub` exposes no
   balance/pedal signal, and nothing in `mainwindow.cpp` connects
   `PowerBalanceDataReceived` / `pedalMetricReceived`. So `wid_2_balancePower`
   and pedal torque/smoothness may never update from a real sensor. VERIFY:
   does any decoder emit these? If not, that's a real bug.
2. **ERG / trainer control**: verify the workout actually emits `setLoad` with
   the right watts in ERG mode (i.e. `trainerControlUserId` got set). This is
   the regression class that motivated this task.

## Harness plan
Add a headless validation pass (mirrors `--screenshots`): drive a demo workout
with a controllable `SimulatorHub`, in ERG, then per sensor type inject a known
value, `processEvents`, and grab a screenshot + log the widget value. Capture
emitted `setLoad`/`setSlope` to prove trainer control fires. Sensor types to
cover: HR, power, cadence, speed, oxygen, L/R balance, pedal metrics, + ERG
load output. Output: one screenshot per type + a pass/fail report.

Extend `SimulatorHub` (or a SensorSimulator) to ALSO emit balance + pedal
metrics so those paths can be exercised once wired.
