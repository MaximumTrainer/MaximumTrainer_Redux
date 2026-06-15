# Zwift virtual shifting — implementation plan & working notes

**Branch:** `zwift-virtual-shifting` (local only — do **not** push until the
distribution/patent question below is resolved).
**Status:** PIVOTED OFF the Zwift protocol — it acks but never actuates
resistance (proven in-saddle for SIM *and* ERG). Virtual shifting is now built
over **standard FTMS** (the channel the app already drives). In-app feature +
a BtleHub-based headless feel test are written; **the next session is to FEEL
it on the trainer** via `--trainer-gear-test`. See "Where we are / resume" below.
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

- **Zwift trainer service:** advertised as **`0000fc82-0000-1000-8000-00805f9b34fb`**
  (16-bit Zwift UUID 0xFC82 — *confirmed via Phase 1 probe on the JetBlack
  Victory fw 4.29*; the earlier `00000001-19ca-…` guess was WRONG). The three
  characteristics inside it do use the 19ca base:
  - Notifications (riding data): `00000002-19ca-4651-86e5-fa29dcdd09d1` [Ntf]
  - Control point (commands):    `00000003-19ca-4651-86e5-fa29dcdd09d1` [Wr|WrNR]
  - Response/indicate:           `00000004-19ca-4651-86e5-fa29dcdd09d1` [Rd|Ind]
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
- [x] **Phase 1 — Read-only probe harness (device).** DONE. `--zwift-probe
  [nameFilter]` (`src/btle/zwift/zwift_probe.*`). Run against the live Victory
  (name "Victory MaJet", C4:67:D8:7A:AC:35, JetBlack fw 4.29):
  `./build/release/MaximumTrainer --zwift-probe Victory` (offscreen ok).
  **Confirmed on hardware:**
  - Zwift service lives under **`0000fc82`** (not `00000001-19ca-…`); chars
    `00000002/3/4-19ca-…` exactly as documented.
  - Handshake: write `"RideOn"` to `…03` → trainer echoes `"RideOn"`
    (`526964654f6e`) on `…04`. **No trailing bytes, NOT encrypted.**
  - `…02` streams `0x03`-framed HubRidingData; the Phase 0 decoder parses it
    cleanly (idle → all-zero fields; needs a pedaling test for non-zero).
  - FTMS (`1826`/`2ad2`) streams in parallel; JetBlack debug char
    `c4632b08` logs ASCII incl. `ResCtrl: Resistance control timer …` and the
    `ANT HR` bridge (the #292 HR path).
  - Zwift Click was not present/paired this session → defer to Phase 3.
- [x] **Phase 2a — Control path validated on hardware (harness).** DONE.
  `--zwift-control [name]` runs a scripted 0x04 sequence after the handshake.
  Live on the Victory: **trainer ACKs our commands** — its ResCtrl debug log
  printed `Target power set to: 250W` / `150W` / `0W` for our ERG writes, and
  resistance ticks reacted. RESET (ERG 0 W) releases cleanly. ERG control over
  the Zwift trainer protocol is proven. (Encoders: ERG `0418fa01`=250W; SIM+gear
  e.g. `04220310b0092a0a10c5a50120a00628cc3a`.)
  - **In-saddle pedaling test #1 (felt nothing, ~22 W flat across all gears).**
    Firmware log (`RfCommsZcs`) proved it RECEIVED our exact gear ratios
    (0.817 / 2.118 / 5.49) + masses — encoding perfect — but resistance never
    changed. ROOT CAUSE: we sent **CWa=0 and Crr=0**. Virtual shifting's feel is
    aero drag (CWa) on the gear-driven virtual speed; with CWa=Crr=0 and grade
    not biting, the gear scales nothing. FIX: send Zwift's fixed
    **CWa=0.51 (5100), Crr=0.004 (400)** (now `ZwiftSim::` constants) in every
    SimulationParam. Gear-sweep grades flattened to 0% so aero isolates gear feel.
  - **In-saddle test #2 (CWa/Crr fix): STILL flat.** Firmware confirmed receipt
    of gear ratios again, but power stayed ~22 W.
  - **In-saddle test #3 (ERG over the Zwift channel): STILL flat.** Power pinned
    ~20 W (independently confirmed via Cycling Power 2A63) through ERG 100/170/
    230 W, all ACKed. **The blue LED was flashing = trainer NOT in a controlled
    session.** ⇒ **The Zwift `fc82` channel acks but never actuates for us.**
    DEAD END. (Likely needs a control-grant the encrypted Zwift app does, or
    actuation just isn't wired to that service on this firmware.)
- [x] **PIVOT → FTMS actuation.** The owner confirmed normal ERG works in the app
  (FTMS), and the app's LED goes **solid**. So drive resistance over standard
  FTMS (`1826`/`2AD9`), the channel `BtleHub` already controls. `--ftms-erg`
  harness gets a real FTMS **control grant** (op00 res01) — but standalone-harness
  FTMS still didn't actuate in saddle (flashing LED) and a dozen rapid
  connect/control cycles left the trainer stuck at free-roll until a power-cycle.
  Conclusion: **don't use the standalone harness — use `BtleHub`** (it gets a
  solid LED + real resistance in the app).
- [x] **Phase 2b — In-app virtual shifting (BUILT; needs in-saddle feel test).**
  Commit `9452aac`+. In `WorkoutDialog`: Up/Down arrows shift a virtual gear
  (`m_virtualGear`, 1..15); on test/slope/free-ride intervals — where the code
  used to send `sendSlopes(0)` (resistance 0, the #293 bug) — it now calls
  `sendGearLoad()` → `setLoad` (FTMS `0x05`) with cadence-aware watts from the
  shared `VirtualGear::targetWatts` model. Re-sent each second so cadence changes
  feel like a gear. Guards: never fights an ERG target, resets mid-gear on start,
  releases on end. Gear shown in the window title (proper on-screen indicator =
  follow-up, do WITH owner). Unit-tested (`tests/zwift`, 20/20).
  - **Headless feel test:** `--trainer-gear-test [name]` drives `BtleHub` (the
    PROVEN path) and auto-cycles gears (warmup → easy↔hard) so the rider just
    pedals and feels them — no source/keyboard needed on the trainer-side PC.
    Smoke test: `FTMS control granted by trainer` + `0x05` acked. THIS is what
    to run in saddle next.
  - NOTE: app is **BLE-only** (ANT+ removed #240). `setLoad(int antID,…)`'s param
    is legacy-named; value is really `trainerControlUserId`/`getFecID()`.
- [ ] **Phase 3 — Zwift Click input (device).** Decode Click button frames →
  call the same shift function. Pure add-on.
  - **Phase 1 recon (Click v2, fw 1.2.0, CC:0A:DF:72:3D:4C) DONE:** advertises
    the **same `0000fc82`** service; chars `00000002/3/4-19ca-…` PLUS three
    extra `00000100/101/102-19ca-…`. mfr "Zwift Inc", hw "B.0", battery 80%.
    After RideOn it streams a handshake incl. `ff03…` + 33 bytes of
    high-entropy data = an **X25519 public key** → **the Click v2 ENCRYPTS its
    button data** (ECDH + AES; the `010x` chars are the secure channel). Unlike
    the trainer, which is plaintext.
  - **Implication:** Click support requires implementing the Zwift crypto
    handshake — real effort AND the **DMCA §1201** circumvention angle. NOT on
    the critical path: keyboard/on-screen shifting (Phase 2) gives full virtual
    shifting with no crypto. Defer Click; decide separately. Public RE exists
    (qdomyos-zwift, makinolo "Zwift Play" posts).
- [ ] **Phase 4 — Polish + distribution call.** Gear indicator UI, gear-range
  persistence; revisit patent/distribution before any merge to public master.

---

## Where we are / how to resume

**The Zwift-protocol path is abandoned** (acks, never actuates). Virtual shifting
is built over **FTMS via `BtleHub`** and is the real #293 fix. Code is on local
branch `zwift-virtual-shifting` (do NOT push — patent question still open).

Key code:
- In-app feature: `src/ui/workoutdialog.{h,cpp}` — `virtualShiftingActive()`,
  `gearTargetWatts()`, `sendGearLoad()`, `shiftGear()`; Up/Down in
  `keyPressEvent`; the two `sendSlopes(0)`→gear swaps (free-ride ~line 1147,
  test/slope ~line 2472); per-second re-send in `sendLastSecondData`.
- Shared model: `src/btle/zwift/virtual_gear.h` (unit-tested, `tests/zwift`).
- Headless feel test: `src/btle/zwift/trainer_gear_test.*` (`--trainer-gear-test`).
- Dormant Zwift harness (kept for reference): `zwift_probe.*`,
  `zwift_protocol.*`, flags `--zwift-probe/-control/-gearsweep`, `--ftms-erg`.

Build & tests (no hardware):
```
qmake6 MaximumTrainer.pro QWT_INSTALL=/tmp/qwt6 && make -j$(nproc)
cd tests/zwift && qmake6 zwift_tests.pro && make && ../../build/tests/zwift_tests   # 20/20
```

**THE NEXT STEP (with owner at the trainer):** feel the gears.
1. Owner: phone Bluetooth OFF (so nothing else holds the trainer); be ready to
   pedal at the trainer.
2. Run from this machine (reaches the trainer over BLE):
   `LD_LIBRARY_PATH=/tmp/qwt6/lib QT_QPA_PLATFORM=offscreen ./build/release/MaximumTrainer --trainer-gear-test Victory`
   (add `--debug` to see `BtleHub` FTMS grant logs). It connects via `BtleHub`,
   warms up ~25 s (mount + pedal), then auto-cycles easy↔hard gears ~90 s and
   releases. Owner reports whether resistance clearly changes per gear; the log
   prints cadence + target watts per gear.
3. If gears are felt → success. Then: tune the gear curve, add an on-screen gear
   indicator + on-screen ▲▼ buttons, decide ratios/persistence, and clean up the
   dormant Zwift harness before any PR.
   If still flat with `BtleHub` (solid LED + real FTMS control) → it's a
   trainer-setup issue (calibration/Cog), not our code.

Gotcha learned: rapid connect→control→disconnect cycles can leave the trainer
stuck at free-roll (LED flashing) — power-cycle the trainer to clear it. Don't
hammer it with the standalone harness.
