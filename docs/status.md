# Status

Legend: **In use** = works on the author's assembled unit; **Bench-checked** = measured or tried once on the bench,
with a multimeter only; **PC-tested** = covered by the tests on the PC, not specifically checked on the device;
**Not validated** = written, never tried.

| Area | State | Notes |
|---|---|---|
| Library (categories, remotes, buttons), learning, sending | In use | Limits and confirmations in `docs/ui.md` |
| Display, five buttons, menus, on-screen keyboard | In use | SSD1306 panel; accents and `ñ ¡ ¿` were not checked glyph by glyph. The SH1106 build compiles but was never tried on such a panel |
| Remotes from the card, import (one remote or a folder), export | In use | Flipper `.ir` files under `/irdb`; `docs/ui.md` |
| Modes: Listen, Be Gone, loop, crazy | In use | |
| Storage in flash: one file per remote, atomic commit, backup, recovery | In use; power cuts PC-tested | Cuts are simulated at every write on the PC; not provoked on the device. `docs/persistence.md` |
| Battery power (18650, charger, boost to 5 V) | In use | Not characterised: behaviour with a nearly empty cell, supply sag while sending |
| Battery level on screen | Bench-checked at one point | 3.8 V on screen against 3.77 V on the multimeter; the percentage is an estimate from a generic curve |
| IR range | Bench-checked | Almost 2 m against a TV with two LEDs; LED current calculated, not measured (`docs/hardware.md`) |
| Codec, domain model <-> IRremoteESP8266 | PC-tested against the real library code | `docs/ir-notes.md`. On the device only NEC remotes were confirmed |
| Manual signals, code exploration | PC-tested | |
| Serial bench console (`bench`), display probe (`display_probe`) | Used during bring-up | Commands in `docs/physical-test-plan.md` |
| Printed case | In use | [On MakerWorld](https://makerworld.com/en/models/3592563-iris-universal-ir-remote-case-esp32-oled#profileId-4115008); `docs/case-print.md` |
| Second receiver | PC-tested | Pin reserved, not wired |
| Second emitter channel, state-based (A/C) protocols, RAW editor on the device | Not implemented | `docs/risks.md` |
| Wokwi project | Not validated | Never opened in Wokwi |

## Tests

```powershell
./tools/test.ps1
```

- `iris_unit_tests`: 212 cases. `iris_ir_tests`: 14 cases (needs the library sources from one `pio run`).
- Firmware environments: `esp32dev`, `esp32dev_ssd1306`, `display_probe`, `bench`.

## Documents and how far to trust them

`docs/wiring.md` and `docs/physical-test-plan.md` were written before the build, as a careful plan; the unit was then
built with a multimeter as the only instrument. Where they say "to verify" or "proposed", that is still true unless
`docs/hardware.md` reports a measurement. `docs/risks.md` lists what remains unknown.
