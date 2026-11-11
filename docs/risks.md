# Risks, limitations and pending work

The author's unit is built and in use, but it was validated with a multimeter and by using it, not with lab
instruments. This is the list of what is still unknown or limited.

## Hardware unknowns (must be measured, not assumed)

| Unknown | Why it matters | How to resolve |
|---|---|---|
| OLED controller | **Resolved for this unit: SSD1306** (display_probe, 2026-10-31). Other panels must be re-identified | `display_probe` environment, `docs/hardware.md` |
| IR LED model and ratings | Sets safe current, pulse width, duty and the `TxSafety` limits | Read markings / datasheet; measure Vf with a multimeter; start with a current-limited test |
| Resistor values | Unconfirmed nominal values | Measure with the LED disconnected; use verified resistors for the first tests |
| Emitter transistor | The IRFZ44N did not switch on from a 3.3 V GPIO on two units, so the unit uses a 2N2222. Its current while sending was calculated, never measured | Measure the LED current before adding LEDs or lowering resistors (`docs/hardware.md`) |
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
- The library moves in and out only as Flipper `.ir` files on the microSD card (import and export); there is no USB or
  wireless transfer.

## Unverified assumptions in the firmware

- LittleFS `rename()` over an existing file is atomic (power-cut test pending; simulated cuts pass on the PC).
- Free heap on the device is enough for the worst-case remote file (PC measurement: < 96 KB text + document;
  the device value is read from Diagnóstico `Heap libre / mínimo`).
- A 10 s task watchdog on the main loop does not trip during display flushes or flash writes.
- Font `u8g2_font_5x8_tf` renders every Spanish glyph used (the menus read well on the panel; accents and `ñ ¡ ¿` were
  not checked one by one).
- Debounce/long-press timings suit the real switches.
- Software PWM jitter is acceptable for the target devices; core 0 busy loops do not trip the watchdog.
- Wokwi project has never been opened.
