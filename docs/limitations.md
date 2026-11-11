# Limitations and open questions

The author's unit is built and in use, but it was validated with a multimeter and by using it, not with lab
instruments. This is what is still unknown, limited or assumed. The state of each part of the project is in
[status](status.md).

## Hardware unknowns

Measure these on your own unit; do not assume them.

| Unknown | Why it matters | How to resolve |
|---|---|---|
| OLED controller | SSD1306 on the author's unit. Another panel may be an SH1106, and that build was never tried on a real panel | [Identify the OLED controller](hardware/bring-up.md#identify-the-oled-controller) |
| IR LED current | The LEDs are TSAL6200 (100 mA continuous per datasheet); the current in use (about 34 mA each) was calculated, not measured. It sets the margin of the `TxSafety` limits | Measure Vf and the real current before adding LEDs or lowering resistors |
| Emitter transistor | The IRFZ44N did not switch on from a 3.3 V GPIO on two units, so the unit uses a 2N2222 | [Design notes](hardware/design-notes.md#choosing-the-transistor) |
| Power path (18650, charger, boost) | The supply while sending was never measured. XL6009 datasheet revisions disagree on its minimum input (3.6 V or 5 V), and the voltage at which it stops with a nearly empty cell is not known. The charger has no power path | Block B9 of the [bring-up](hardware/bring-up.md); charge with the unit off; check *Settings > Diagnostics > Boot history* for `BROWNOUT` |
| TSOP38238 | Pinout 1 = OUT, 2 = GND, 3 = VS and supply 2.0-5.5 V were read in the Vishay datasheet, but your parts may be clones or breakout boards with another order. Sony codes may decode less reliably with this version (datasheet) | Package drawing, blocks B1 and B5 |
| Battery measurement | The ADC is non-linear and only one point was compared with a multimeter; the percentage is an estimate from a generic curve, and the voltage at which the boost converter stops is not known | Compare near 3.3 V and 4.2 V; the diagnostics page shows the exact voltage next to the estimate |

## Software limitations

- RAW signals cannot be typed on the device; they come from a capture.
- No state-based (air-conditioner) protocols; only NEC, Samsung, Sony, RC5, JVC are mapped.
- Learned signals store one frame; held-key repeat patterns are not captured.
- Cancel granularity is one library call (frame or Sony burst).
- Exploration cannot know whether the target reacted; it is bounded (<= 1000 codes, >= 250 ms gap,
  confirmation above 50 codes).
- Second receiver: the code path exists (`MultiReceiver`, `IRIS_ENABLE_RX2`) but it was never wired. A second emitter
  channel has a pin reserved and no code.
- The device cannot be used while it charges (no power path in the charger module).
- One backup generation (`idx.prev`). If both indexes are damaged the device starts empty and keeps the
  unreadable bytes as `badidx.irs`; one damaged remote file loses only that remote ([persistence](firmware/persistence.md)).
- The library moves in and out only as Flipper `.ir` files on the microSD card (import and export); there is no USB or
  wireless transfer.

## Assumptions in the firmware that were not verified

- LittleFS `rename()` over an existing file is atomic (cuts are simulated on the PC at every write; none was provoked
  on the device).
- Free heap on the device is enough for the worst-case remote file (PC measurement: < 96 KB text + document;
  the device value is on *Settings > Diagnostics > Status*).
- A 10 s task watchdog on the main loop does not trip during display flushes or flash writes.
- Font `u8g2_font_5x8_tf` renders every Spanish glyph used (the menus read well on the panel; accents and `ñ ¡ ¿` were
  not checked one by one).
- Debounce/long-press timings suit the real switches.
- Software PWM jitter is acceptable for the target devices; core 0 busy loops do not trip the watchdog.
- The Wokwi project in `wokwi/` has never been opened.
