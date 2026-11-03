# Status (end of Stage 7)

Legend: **Functional** = implemented and covered by PC tests; **Cross-checked** = verified against the real
IRremoteESP8266 code on the PC; **Compiles** = builds for the ESP32 but has never run on it; **Simulated** = exists
only as a fake; **Documented** = guide written, nothing executed. Nothing here has run on real hardware.

| Area | State | Notes |
|---|---|---|
| Domain, library rules, manual signals, learn / transmit / exploration | Functional | Limits and confirmations in `docs/ui.md` |
| Codec domain <-> IRremoteESP8266 | Cross-checked | 7464 assertions; findings in `docs/ir-notes.md` |
| Persistence: per-remote files, atomic commit, backup, recovery, versioning | Functional | `docs/persistence.md`; power cuts simulated at every write |
| Boot logic, boot history, settings, memory guard | Functional | Real heap numbers unmeasured |
| UI (all screens, editors, forms), debouncer | Functional (fake display) | 147 PC test cases; never seen on a real OLED |
| Multi-receiver | Functional | Second receiver not wired |
| IR receiver / transmitter adapters, LittleFS store, U8g2 display, GPIO buttons, watchdog | Compiles | Never run; provisional safety limits |
| **Serial bench console** (`bench` env: I2C scan, buttons, RX dump, bounded TX, loopback, ADC) | Compiles | New in Stage 7; commands in `docs/physical-test-plan.md` |
| Display probe (`display_probe` env) | Compiles | |
| **Wiring guide** (`docs/wiring.md`) | Documented | Starting-point values; pinout of the TSOP38238 read in the Vishay datasheet; several items marked to verify |
| **Physical test plan** (`docs/physical-test-plan.md`, B0-B11) with log template | Documented | Never executed; pass criteria marked "proposed" |
| Console simulator, fakes | Simulated | |
| Wokwi | Prepared, unvalidated | Never opened |
| Battery measurement (`IBatteryMonitor` for the divider) | Bench-checked at one point | 3.8 V on screen against 3.77 V on the multimeter (2026-11-03); other voltages not compared. The title bar shows an estimated level and bars (simulated, flashed on the panel pending) |
| Second emitter channel, state-based (A/C) protocols, RAW editor on the device, library export | Pending | Listed in `docs/risks.md` |

## Tests

```powershell
./tools/test.ps1
```

- `iris_unit_tests`: 147 cases.  `iris_ir_tests`: 10 cases (needs the library sources from one `pio run`).
- Firmware: `esp32dev`, `esp32dev_ssd1306`, `display_probe`, `bench` all build.

## What the first hardware session should produce

The acceptance list in `docs/physical-test-plan.md` (B11). Until it is ticked, treat every hardware-dependent
statement in this repository as an assumption.
