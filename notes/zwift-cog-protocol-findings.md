# Zwift Cog / virtual shifting — protocol findings & Click roadmap

Consolidated reference so the next person (or the Zwift Click work) doesn't start
from scratch. Companion to `zwift-virtual-shifting-plan.md` (the journey log).
Hardware used: **JetBlack Victory** trainer (fw 4.29) + **Zwift Click v2** (fw 1.2.0).

---

> **STATUS (2026-06-16): Zwift Click controller support was REMOVED from the
> product** (branch `strip-zwift-click`, closes the relay path of #300). The FC82
> Click relay shipped, then broke ERG: reading the Click through the trainer
> requires writing Zwift's `RideOn` to the trainer's FC82 control point, which
> opens a Zwift control session and makes the trainer **stop servicing the FTMS
> control point** (`Request Control` times out → no ERG, stuck ~120 W). It fired
> on every connect to a Zwift-protocol trainer, Click or no Click.
>
> **Why it can't coexist:** Zwift trainers expose a *proprietary* control service
> (not FTMS) that is the trainer's **single control plane** — ERG + SIM + gear
> ratios all flow through it (makinolo, "Zwift trainer protocol", 2024-10-20).
> FTMS and the Zwift session are mutually exclusive on the trainer, and the
> proprietary protocol is undocumented + a moving target (Zwift changed the
> controller service UUID to FC82 in Jan 2025). We keep the **standard FTMS path**
> (ERG = `0x05`, virtual shifting = `0x04`, shifted by ▲/▼ keys / on-screen).
>
> **The only ERG-safe way to read the Click** is a *direct* BLE connection to the
> controller (never touching the trainer) — parked as a future spike because it
> was unreliable (LED never solid, mid-session drops). Everything below is kept
> as the reverse-engineering reference for that future attempt; the protocol is
> public (makinolo blog), so the findings are not sensitive.

---

## TL;DR / what shipped

Virtual shifting (#293) ships over **standard FTMS Set Target Resistance Level
(`0x04`)**, NOT Zwift's proprietary protocol. Each virtual gear → a fixed
resistance level → instant, real-gear feel. Opt-in setting (default off) so
normal-cassette trainers are unaffected. Up/Down keys + on-screen ▲/▼ shift.

The Zwift proprietary protocol was explored and **abandoned** (see below): the
trainer ACKs its commands but never actuates resistance for us.

---

## Zwift proprietary *trainer* protocol (the Victory) — UNENCRYPTED

Discovered via `--zwift-probe`. The trainer exposes a Zwift service **advertised
as 16-bit `0x FC82`** (`0000fc82-0000-1000-8000-00805f9b34fb`), containing:

| Characteristic | UUID | Props | Role |
|---|---|---|---|
| Measurement | `00000002-19ca-4651-86e5-fa29dcdd09d1` | Notify | riding data (0x03 frames) |
| Control point | `00000003-19ca-4651-86e5-fa29dcdd09d1` | Wr/WrNR | commands (0x04) |
| Response | `00000004-19ca-4651-86e5-fa29dcdd09d1` | Rd/Ind | handshake/cmd responses |

- **Handshake:** write ASCII `"RideOn"` to the control point → trainer echoes
  `"RideOn"` (`526964654f6e`) on the response char. **No encryption**, no trailing
  bytes.
- **Command `0x04` (HubCommand, protobuf):** field 3 `PowerTarget` (uint32),
  field 4 `SimulationParam { f1 wind sint32, f2 inclineX100 sint32, f3 CWa uint32
  =CW·a·10000, f4 Crr uint32 =Crr·100000 }`, field 5 `PhysicalParam { f2
  GearRatioX10000 uint32, f4 BikeWeightx100, f5 RiderWeightx100 }`.
  Zwift fixes **CWa=0.51 (5100), Crr=0.004 (400)**.
- **Incoming `0x03` (HubRidingData):** f1 power, f2 cadence, f3 speedX100, f4 hr.
- Codec implemented + unit-tested: `src/btle/zwift/zwift_protocol.h`.

**Why abandoned:** on the Victory, the trainer **acknowledges** every `0x04`
(SIM gear ratio *and* ERG power — its `RfCommsZcs`/`ResCtrl` debug log even prints
"Set simulated virtual gear ratio received: 5490" and "Target power set to: 250W")
**but applies zero resistance** — power stays ~20 W in saddle, blue LED flashing
(= not in a controlled session). FTMS, by contrast, actuates fine (solid LED).
Likely needs a control-grant the real (encrypted) Zwift app performs, or actuation
simply isn't wired to this service on this firmware. Not worth fighting — FTMS works.

JetBlack diagnostic char `c4632b08-003f-4cec-8994-e489b04d857f` streams ASCII logs
(incl. `ResCtrl:` resistance lines and the `ANT HR` bridge) — invaluable as a bench
instrument.

---

## Zwift Click v2 (fw 1.2.0) — ENCRYPTED (the Phase 3 blocker)

Discovered via `--zwift-probe` (`CC:0A:DF:72:3D:4C`, mfr "Zwift Inc", hw "B.0"):

- Advertises the **same `0x FC82`** service, with chars `00000002/3/4-19ca-…`
  **plus three more: `00000100/101/102-19ca-…`** (the secure channel).
- After `RideOn` it emits an `ff03…` handshake carrying **33 bytes of high-entropy
  data = an X25519 public key** → **the Click v2 encrypts its button data**
  (ECDH key exchange + AES). The *trainer* link is plaintext; the *Click* is not.

**Implication for Click support (future):** must implement the Zwift encrypted
handshake (X25519 → shared secret → AES-GCM decrypt of button events). Real effort
+ a **DMCA §1201** circumvention angle (the trainer path had none — it's plaintext).
The Click is only an INPUT device (it tells the app to shift; it never talks to the
trainer), so it's fully decoupled from actuation: **keyboard/on-screen shifting
already delivers the feature; the Click is an optional, separable add-on.**

Decoupled architecture to keep:
```
Zwift Click button → app virtual-gear index ±1 → app computes resistance → FTMS 0x04
   (encrypted input)          (app logic)                                  (actuation, done)
```

---

## Exploration tools left in the tree (dev/debug, behind CLI flags)

- `--zwift-probe [name]` — read-only BLE GATT dump + RideOn + frame decode
  (`zwift_probe.*`). How the above was found; rerun for the Click crypto work.
- `--zwift-control [name]` / `--zwift-gearsweep` — drive 0x04 on the Zwift service
  (proved acks-but-no-actuation).
- `--ftms-erg [name]` / `--trainer-gear-test [name]` — drive FTMS via BtleHub (the
  working path; `--trainer-gear-test` auto-cycles gears for an input-free feel test).

(These are dormant in normal use; strip or keep before any public PR.)

---

## References
- Makinolo, "Zwift Trainer protocol" (2024-10-20) and "Virtual gear shifting"
  (2023-11-06) — the protocol/protobuf field map.
- qdomyos-zwift (GPLv3) — working Zwift-protocol + Click crypto implementation.
- Zwift Insider, "All About Virtual Shifting" — gear range (~0.75–5.49, 24 gears).
- Patent to check before public distribution: **US 11,986,700**, "Virtual shifting
  for exercise devices" (covers the *method*, independent of protocol).
