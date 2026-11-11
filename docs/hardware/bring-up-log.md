# Bring-up log (copy this file for each session)

The blocks are those of the [bring-up plan](bring-up.md).

Date: ____  Tester: ____  Firmware (`git`/build id or date): ____  Environment: `esp32dev` / `esp32dev_ssd1306` / `bench`

Equipment and instruments used (model, accuracy if known): ____

Rule: write what you **measured**, not what you expected. Use "n/m" for not measured. Attach photos and
scope captures by file name.

## Parts as found (B1)

| Part | Marking / source | Measured value | Notes |
|---|---|---|---|
| R1 ... | | | |
| IR LED #1 | | Vf @ 1 mA = | |
| Driver transistor #1 | | | |
| TSOP #1 | | | |
| Cell | | V = | protected? |
| Charge module | chips: | | |
| DC-DC module | chip: | | |

## Results

| Block | Step | Expected (from the plan) | Measured | Pass / Fail | Notes, file names |
|---|---|---|---|---|---|
| B2 | 3V3 rail | 3.2-3.4 V | | | |
| B2 | GPIO23 at boot | ~0 V | | | |
| B3 | I2C address | 0x3C / 0x3D | | | |
| B3 | Clean driver | SH1106 / SSD1306 | | | |
| B4 | Keys 5/5 | all correct | | | |
| B5 | Idle OUT level | ~3.3 V | | | |
| B5 | `l` edges in quiet | ~0 | | | |
| B5 | Reference remote decoded | protocol / address / command | | | |
| B6 | Emitter off at boot / reset | yes | | | |
| B6 | Transistor drop at ___ mA | <= 0.2 V | | | |
| B6 | Carrier frequency | ~38 kHz +/-5 % | | | |
| B7 | LED resistor used | | | | |
| B7 | DC current | <= target | | | |
| B7 | Loopback max distance (OK) | | | | |
| B8 | Target device response (per distance/angle) | | | | |
| B9a | OUT+/OUT- no load | = cell voltage | | | |
| B9b | Min input that regulates | | | | |
| B9c | 30 min on battery | no resets | | | |
| B9d | Min 3.3 V rail while sending | > 3.0 V | | | |
| B9d | BROWNOUT entries in 100 sends | 0 | | | |
| B9e | End-of-charge voltage | ~4.2 V | | | |
| B10 | Power cuts: ok / lost / damaged | all ok | | | |
| B10 | Heap free / min | | | | |
| B10 | 1 h endurance | no resets | | | |

## Decisions and changes made as a result

| Decision | Reason (measurement) | File changed |
|---|---|---|
| | | |

## Open problems

-
