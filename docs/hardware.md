# Hardware notes

The complete guides are `docs/wiring.md` (how to connect each block) and `docs/physical-test-plan.md`
(how to validate it, block by block, with the `bench` console). This file keeps the display procedure and
the earlier Stage 5 checklist, which the test plan supersedes.

Nothing in this file has been verified on the real board yet.

## Identify the OLED controller (do this first)

**Result for the author's unit (2026-10-31): SSD1306.** The SSD1306 test picture was clean; see
`docs/logs/2026-10-31-b2-b3.md`. The procedure below is kept for any other panel.

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

## Pins (proposed, unverified)

See `include/board_pins.h`. Avoided on purpose: GPIO 6-11 (flash), 0/2/5/12/15 (strapping, GPIO12 can
stop the boot), 1/3 (USB serial). GPIO 34-39 are input-only without pull-ups (used for the IR
receivers). Verify against your board's silkscreen: 30-pin boards differ.

## Buttons

Each button between its GPIO and GND (internal pull-up). Use *Configuración > Prueba de botones* to
check all five: it shows the live level of every key and the last event (short / long press).

## Stage 5 checks to run on the board (in this order; stop at the first surprise)

1. **Storage and memory**: boot, create a control, reboot: it must still be there (*Configuración > Almacenamiento*
   shows `Flash nK/nK`). If it says `FLASH NO MONTADA` the partition is unformatted: format from that screen.
   Power-cut test of the atomic write: save repeatedly while removing power at random moments; after each
   boot the library must load from the current or the backup index (the boot toast says which). Note the
   `Heap` line in *Diagnóstico HW* before and after: it is the real number behind the memory budget.
   *Herramientas IR > Historial arranques* lists the last reset reasons (BROWNOUT, WDT, PANIC...).
2. **Receiver only** (emitter not connected): *Herramientas IR > Captura RAW*, point a known remote at the
   TSOP, press a key. Expect `Reconocida` for NEC/Samsung/Sony/RC5/JVC remotes and RAW otherwise. Holding
   the key must not produce a second capture. Compare the decoded address/command with documentation for
   that remote when available. Check *Diagnóstico HW* counters (`RX vistos`).
3. **Emitter, no LED**: connect only the MOSFET gate to a scope/logic analyser or a multimeter in AC mode and send
   a code: the gate must be low at boot, show bursts only while sending, and end low. Check that a mark never
   exceeds 25 ms and that *Cancelar* stops between frames.
4. **Emitter with a current-limited LED**: use a verified resistor sized for a conservative current from the
   LED's measured forward voltage. See the LED glow with a phone camera. Do not add LEDs or raise limits yet.
5. **Self-capture**: with the emitter on, point it at the TSOP and compare what is captured against what was
   stored (the receiver is muted while sending, so do this with a second capture after the send).
6. **Power**: watch *Diagnóstico HW > Reinicio* after repeated sends; a `BROWNOUT` reset means the supply sags
   during emission (bulk capacitor, separate IR rail).
