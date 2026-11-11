# Hardware notes

The complete guides are `docs/wiring.md` (how to connect each block) and `docs/physical-test-plan.md`
(how to validate it, block by block, with the `bench` console). This file keeps the display procedure and
the earlier Stage 5 checklist, which the test plan supersedes.

## Measured on the author's unit

A summary of the bench sessions (2026-10-31 to 2026-11-03). Multimeter only, no oscilloscope: currents are calculated
from resistor values unless a row says otherwise, and distances are estimates.

| Block | Result |
|---|---|
| Board | 30-pin ESP32-WROOM-32 dev board, CP210x USB-serial. The pins of `include/board_pins.h` were used as they are |
| OLED | 1.3" 128x64 I2C, **SSD1306** (identified with `display_probe`); the menus read well on the panel |
| Receiver | TSOP38238 on GPIO34; NEC remotes are decoded and learned through the UI |
| Emitter driver | Two IRFZ44N units did **not** switch on from the 3.3 V GPIO (gate at 3.2 V, drain barely moving). Replaced by a 2N2222: GPIO23 -> 1 kΩ -> base, emitter to GND, collector to the LED cathodes |
| Emitter range (a TV as the target) | 1 LED with 220 Ω (about 16 mA): a few cm. 1 LED at about 32 mA: under 1 m. **2 LEDs, each with its own 100 Ω from 5 V (about 34 mA each): almost 2 m** |
| microSD | 6-pin SPI module powered from VIN (5 V), read at 4 MHz on SCK 18 / MISO 16 / MOSI 17 / CS 4 |
| Power | One 18650, TP4056-class USB-C charger with protection, XL6009 boost set to 5.0 V into VIN. With the cell at 3.87 V the board ran at 4.96 V. Not tested: a nearly empty cell, long runs |
| Battery reading | Divider of 2 x 10 kΩ to GPIO33 (`IRIS_BATTERY_RATIO_PERMILLE=2000`): 3.8 V on screen against 3.77 V on the multimeter, one point only |

A lesson worth repeating: a loose breadboard contact on the emitter drive looked for a whole session like a range
problem. Check that the drive pin of the transistor really moves before judging the range.

## Identify the OLED controller (do this first)

**Result for the author's unit (2026-10-31): SSD1306** (its test picture was clean, the SH1106 one was not). The
procedure below is for any other panel.

An I2C scan only returns the address (0x3C or 0x3D); it cannot tell SH1106 from SSD1306. A 1.3"
128x64 module is often SH1106 (132-column RAM, 2 px offset), but that is a guess, not a fact.

1. Wire VCC -> 3V3, GND -> GND, SDA -> GPIO21, SCL -> GPIO22 (see `include/board_pins.h`).
2. Flash the probe: `.\.venv\Scripts\pio.exe run -e display_probe -t upload`, then open the monitor
   (`pio device monitor`). It prints the I2C scan and alternates the same test picture between the
   SH1106 driver and the SSD1306 driver every 6 s.
3. The right driver shows a complete border touching all four edges, two clean diagonals and sharp text.
   The wrong one is typically shifted ~2 px, has a column of noise at an edge, or wraps a line.
4. Build the matching firmware: `esp32dev` (SH1106) or `esp32dev_ssd1306`.

If the scan finds nothing, check the 4 wires and the supply before suspecting the driver.

## Pins

See `include/board_pins.h`. Avoided on purpose: GPIO 6-11 (flash), 0/2/5/12/15 (strapping, GPIO12 can
stop the boot), 1/3 (USB serial). GPIO 34-39 are input-only without pull-ups (used for the IR
receivers). Verify against your board's silkscreen: 30-pin boards differ.

## Buttons

Each button between its GPIO and GND (internal pull-up). Use *Configuración > Diagnóstico > Prueba de botones* to
check all five: it shows the live level of every key and the last event (short / long press).

## Checks to run on a new board (in this order; stop at the first surprise)

1. **Storage and memory**: boot, create a control, reboot: it must still be there (*Configuración > Almacenamiento*
   shows `Flash nK/nK`). If it says `FLASH NO MONTADA` the partition is unformatted: format from that screen.
   Power-cut test of the atomic write: save repeatedly while removing power at random moments; after each
   boot the library must load from the current or the backup index (the boot toast says which). Note the
   `Heap` line in *Configuración > Diagnóstico > Estado* before and after: it is the real number behind the memory budget.
   *Configuración > Diagnóstico > Historial arranques* lists the last reset reasons (BROWNOUT, WDT, PANIC...).
2. **Receiver only** (emitter not connected): *Escuchar*, point a known remote at the
   TSOP, press a key. Expect `Reconocida` for NEC/Samsung/Sony/RC5/JVC remotes and RAW otherwise. Holding
   the key must not produce a second capture. Compare the decoded address/command with documentation for
   that remote when available. Check the *Configuración > Diagnóstico > Estado* counters (`RX vistos`).
3. **Emitter, no LED**: connect only the transistor's drive pin to a scope/logic analyser or a multimeter in AC mode and
   send a code: it must be low at boot, show bursts only while sending, and end low. Check that a mark never
   exceeds 25 ms and that *Cancelar* stops between frames.
4. **Emitter with a current-limited LED**: use a verified resistor sized for a conservative current from the
   LED's measured forward voltage. See the LED glow with a phone camera. Do not add LEDs or raise limits yet.
5. **Self-capture**: with the emitter on, point it at the TSOP and compare what is captured against what was
   stored (the receiver is muted while sending, so do this with a second capture after the send).
6. **Power**: watch *Configuración > Diagnóstico > Estado* (`Reinicio`) after repeated sends; a `BROWNOUT` reset means the supply sags
   during emission (bulk capacitor, separate IR rail).
