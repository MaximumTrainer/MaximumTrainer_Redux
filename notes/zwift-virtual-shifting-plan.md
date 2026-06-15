# Zwift virtual shifting — implementation plan & working notes

**Branch:** `zwift-virtual-shifting` (local only — do **not** push until the
distribution/patent question below is resolved).
**Status:** Phase 0 in progress.
**Origin:** issue [#293](https://github.com/MaximumTrainer/MaximumTrainer_Redux/issues/293)
— JetBlack Victory + Zwift Cog: trainer "resistance was 0" during an FTP/MAP
test interval and the rider could not pedal.

---

## Why this is needed (root cause of #293)

The FTP/MAP **test interval** is deliberately driven in **slope/SIM mode at 0 %
grade**, not ERG:

`src/ui/workoutdialog.cpp:2529`
```cpp
if (interval.isTestInterval() || interval.getPowerStepType() == Interval::NONE
    || workout.getWorkoutNameEnum() == Workout::OPEN_RIDE)
    isUsingSlopeMode = true;     // → sendSlopes(0) → FTMS 0x11, grade = 0%
```

On a normal trainer with a real cassette, 0 % grade is fine — you shift to a hard
gear and push. The **Zwift Cog is a single sprocket** (one gear ratio), so at 0 %
simulated grade there is almost nothing to push against → spin-out → "resistance
was 0." Single-cog trainers *require* virtual shifting (or ERG) to have usable
resistance in SIM mode.

We chose to implement **real virtual shifting** (option 3) rather than change the
test (option 1) or a manual FTMS-resistance approximation (option 2), because
Maxime owns the Zwift Cog + Click hardware to test with.

---

## Key protocol facts (from public reverse-engineering)

Virtual shifting on the Victory does **NOT** run over FTMS. It uses Zwift's own
**proprietary trainer protocol**, and the gear→resistance math lives in the
**trainer firmware** — we send a gear ratio, the trainer computes resistance.

- **Zwift trainer service:** `00000001-19ca-4651-86e5-fa29dcdd09d1`
  - Notifications (riding data): `00000002-19ca-4651-86e5-fa29dcdd09d1`
  - Control point (commands):    `00000003-19ca-4651-86e5-fa29dcdd09d1`
  - Response/indicate (assumed): `00000004-19ca-4651-86e5-fa29dcdd09d1`
- **Framing:** `[command_code][protobuf_data]`
- **Incoming 0x03 — HubRidingData:** f1 Power, f2 Cadence, f3 SpeedX100,
  f4 HR, f5/f6 unknown (all uint32 varints).
- **Outgoing 0x04 — HubCommand:**
  - f3 PowerTarget (uint32) — ERG
  - f4 SimulationParam { f1 Wind sint32 (m/s·100), f2 InclineX100 sint32,
    f3 CWa uint32 (CW·a·10000), f4 Crr uint32 (·100000) }
  - f5 PhysicalParam { f2 **GearRatioX10000** uint32, f4 BikeWeightX100 uint32,
    f5 RiderWeightX100 uint32 }
- **Encryption:** the trainer link is **NOT encrypted** (same as Zwift Ride).
  Handshake begins with `"RideOn"` — trailing bytes TBD, confirm in Phase 1.
- **Virtual gear range:** ~0.75 → 5.49, 24 gears (Zwift Insider).
- The **Zwift Click** is a *separate* BLE input device — it tells the *app* to
  shift up/down; it never talks to the trainer. Its button protocol is decoded
  in Phase 3; keyboard/on-screen shifting covers Phases 0–2.

Sources: Makinolo "Zwift Trainer protocol" (2024-10-20) and "Virtual gear
shifting" (2023-11-06); Zwift Insider "All About Virtual Shifting";
qdomyos-zwift (GPLv3), SHIFTR.

---

## Legal / distribution notes (resolve before pushing)

- **License:** project is GPL-3.0; qdomyos-zwift reference is GPLv3 → reuse is
  fine if GPL terms respected. Verify any snippet's license before copying.
- **Patent:** Zwift holds **US 11,986,700 "Virtual shifting for exercise
  devices."** Patents cover the *method*, not the code — clean-room does not
  help. **Read claim 1 before any public distribution.** Local experimentation
  is low-risk; *public release* is the exposure point.
- **DMCA §1201:** only a concern if firmware required circumventing encryption —
  it does **not** (link is unencrypted), so this risk is low here.
- Keep the Zwift-proprietary path in a **separable module** (`src/btle/zwift/`)
  so it can stay on an experimental branch if the patent question blocks a merge.

---

## Phased plan

- [ ] **Phase 0 — Protocol scaffolding (no device).** `src/btle/zwift/`
  protocol constants + protobuf codec for HubCommand (0x04) and HubRidingData
  (0x03), the gear table, handshake constant. Unit-tested against byte fixtures.
  ← *current*
- [ ] **Phase 1 — Read-only probe harness (device).** CLI flag (extend the
  `--sensor-check` pattern) that connects to the Victory + Click, dumps
  services/characteristics, runs the RideOn handshake, logs raw + decoded
  frames. Confirms layout, handshake trailing bytes, encryption assumption,
  Click button format. Read-only = safe.
- [ ] **Phase 2 — Trainer control path (device).** `ZwiftTrainerController`
  sibling to `setLoad`/`setSlope`; `setVirtualGear(riderId, gearIndex)` →
  encode 0x04 with gear ratio + sim params + rider/bike weight → write control
  point. Drive with keyboard / on-screen ▲▼ first (no Click dependency). Wire
  into workout SIM mode.
  - NOTE: the app is **BLE-only** — ANT+ was removed in #240. The existing
    `setLoad(int antID, …)` / `setSlope(int antID, …)` params are named `antID`
    purely as legacy leftover; the value is really the BLE trainer/rider id
    (`trainerControlUserId` / `getFecID()`). New API uses `riderId`, not `antID`.
- [ ] **Phase 3 — Zwift Click input (device).** Decode Click button frames →
  call the same shift function. Pure add-on.
- [ ] **Phase 4 — Polish + distribution call.** Gear indicator UI, gear-range
  persistence; revisit patent/distribution before any merge to public master.

---

## Resume checklist

1. `git checkout zwift-virtual-shifting`
2. Phase 0 code lives in `src/btle/zwift/`; tests in `tests/zwift/`.
3. Build the unit tests: `cd tests/zwift && qmake6 zwift_tests.pro && make && ./zwift_tests -v2`
4. Next up = Phase 1 probe harness against real hardware.
