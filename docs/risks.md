# Risks, limitations and pending work

Updated at the end of Stage 7. Nothing has been run on the real board.

## Hardware unknowns (must be measured, not assumed)

| Unknown | Why it matters | How to resolve |
|---|---|---|
| OLED controller | **Resolved for this unit: SSD1306** (display_probe, 2026-10-31). Other panels must be re-identified | `display_probe` environment, `docs/hardware.md`, `docs/logs/2026-10-31-b2-b3.md` |
| IR LED model and ratings | Sets safe current, pulse width, duty and the `TxSafety` limits | Read markings / datasheet; measure Vf with a multimeter; start with a current-limited test |
| Resistor values | Unconfirmed nominal values | Measure with the LED disconnected; use verified resistors for the first tests |
| MOSFET | IRFZ44N: threshold 2-4 V and Rds(on) specified at VGS = 10 V (distributor datasheet figures), so not guaranteed at 3.3 V | Measure Vds on every unit in B6, or use a logic-level part whose datasheet you read (`docs/wiring.md` 4.5) |
| Power path (18650, charger, boost) | Brownouts during emission. XL6009 datasheet revisions disagree on the minimum input (3.6 V vs 5 V). A TP4056-class charger must not feed the load while charging | Bench test B9b, power-path stage or "off while charging", separate IR rail, bulk capacitors, reset reason in *Historial arranques* |
| TSOP38238 | Pinout 1 = OUT, 2 = GND, 3 = VS and supply 2.0-5.5 V were read in the Vishay datasheet, but your parts may be clones or breakout boards with another order. Sony codes may decode less reliably with this version (datasheet) | Package drawing + B1/B5 |
| Battery measurement | The ADC is non-linear and only one point was compared with a multimeter; the percentage is an estimate from a generic curve, and the voltage at which the boost converter stops is not known | Compare near 3.3 V and 4.2 V; the UI marks the level with `~` and shows the exact voltage on the diagnostics page |

## Software limitations

- RAW signals cannot be typed on the device; they come from capture. An external editor is future work.
- No state-based (air-conditioner) protocols; only NEC, Samsung, Sony, RC5, JVC are mapped.
- Learned signals store one frame; held-key repeat patterns are not captured.
- Cancel granularity is one library call (frame or Sony burst).
- Exploration cannot know whether the target reacted; it is bounded (<= 1000 codes, >= 250 ms gap,
  confirmation above 50 codes).
- Second receiver: the code path exists (`MultiReceiver`, `IRIS_ENABLE_RX2`) but it is not wired/tested.
- One backup generation (`idx.prev`). If both indexes are damaged the device starts empty and keeps the
  unreadable bytes as `badidx.irs`; one damaged remote file loses only that remote (`docs/persistence.md`).
- No export/import of the library to a PC.

## Unverified assumptions in the firmware

- LittleFS `rename()` over an existing file is atomic (power-cut test pending; simulated cuts pass on the PC).
- Free heap on the device is enough for the worst-case remote file (PC measurement: < 96 KB text + document;
  the device value is read from Diagnóstico `Heap libre / mínimo`).
- A 10 s task watchdog on the main loop does not trip during display flushes or flash writes.
- Font `u8g2_font_5x8_tf` renders every Spanish glyph used; UI layout is readable on a 1.3" panel.
- Debounce/long-press timings suit the real switches.
- Software PWM jitter is acceptable for the target devices; core 0 busy loops do not trip the watchdog.
- Wokwi project has never been opened.
