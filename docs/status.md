# Status

What works, and how each part was checked.

- **In use**: works on the author's assembled unit.
- **Bench-checked**: measured or tried once on the bench, with a multimeter only.
- **PC-tested**: covered by the tests on the PC, not specifically checked on the device.
- **Not validated**: written, never tried.

| Area | State | Notes |
|---|---|---|
| Library (categories, remotes, buttons), learning, sending | In use | [Interface](firmware/ui.md) |
| Display, five buttons, menus, on-screen keyboard | In use | SSD1306 panel. Accents and `ñ ¡ ¿` were not checked glyph by glyph. The SH1106 build compiles but was never tried on such a panel |
| Remotes from the card, import (one remote or a folder), export | In use | Flipper `.ir` files under `/irdb` |
| Modes: Listen, Be Gone, loop, crazy | In use | |
| Storage in flash: one file per remote, atomic commit, backup, recovery | In use; power cuts PC-tested | Cuts are simulated at every write on the PC, not provoked on the device. [Persistence](firmware/persistence.md) |
| Battery power (18650, charger, boost to 5 V) | In use | Not characterised: a nearly empty cell, supply sag while sending |
| Battery level on screen | Bench-checked at one point | 3.8 V on screen against 3.77 V on the multimeter. The percentage is an estimate from a generic curve |
| IR range | Bench-checked | Almost 2 m against a TV with two LEDs. LED current calculated, not measured. [Measurements](hardware/measurements.md) |
| Codec: domain model to and from IRremoteESP8266 | PC-tested against the real library code | On the device only NEC remotes were confirmed. [IR layer](firmware/ir.md) |
| Manual signals, code exploration | PC-tested | |
| Serial bench console (`bench`), display probe (`display_probe`) | Used during bring-up | [Bring-up](hardware/bring-up.md) |
| Printed case | In use | [Case](hardware/case.md), also on [MakerWorld](https://makerworld.com/en/models/3592563-iris-universal-ir-remote-case-esp32-oled) |
| Second receiver | PC-tested | Pin reserved, not wired |
| Second emitter channel, state-based (A/C) protocols, RAW editor on the device | Not implemented | |
| Wokwi project | Not validated | Never opened in Wokwi |

What is unknown or limited is listed in [limitations](limitations.md).

## Tests

```powershell
./tools/test.ps1
```

- `iris_unit_tests`: 212 cases. `iris_ir_tests`: 14 cases (needs the library sources from one `pio run`).
- Firmware environments: `esp32dev_ssd1306` (default), `esp32dev`, `display_probe`, `bench`.

A passing PC test never means "works on hardware".
