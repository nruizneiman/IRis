# Physical test plan (by blocks)

Nothing in this plan has been executed. Desktop tests and the simulator are **not** a substitute for any of
it: they prove the software logic, not electrons or light. Pass/fail numbers marked *(proposed)* are starting
criteria chosen for this project, not figures from a datasheet; adjust them when you learn more, and write the
change in the log.

Rule: **do not start block N+1 until block N passes.** Wire only what the block needs. Record every result in
`docs/test-log-template.md` (copy it per session). If something fails, stop and use the "If it fails" line
before continuing; do not "fix it later in firmware".

## Tools

| Tool | Needed for | Required? |
|---|---|---|
| Multimeter (DC V, resistance, diode mode, mA range) | everything | **Yes** |
| Phone camera (many show IR as a faint purple glow) | LED alive check | Useful |
| Oscilloscope or logic analyser (>= 1 MHz sampling) | gate waveform, 38 kHz, supply sag | Strongly recommended for B6, B7, B9 |
| A TV/AV remote of a common protocol (NEC / Sony / RC5 / Samsung / JVC) | receiver, learn, replay | **Yes** |
| A target device you own that responds to that remote | range / aim | For B8 |
| Bench supply or the USB 5 V | stable supply for B6-B8 | USB is enough |
| Spare resistors you have measured | R_led, Rg, Rpd, dummy load | **Yes** |

Software: `pio run -e bench -t upload` (serial console, 115200 baud; type `h`), `display_probe`, and the
`esp32dev` / `esp32dev_ssd1306` firmware. `pio device monitor` for the console.

### Bench console commands

| Command | What it does |
|---|---|
| `h` | help |
| `m` | reset reason, free heap, pin map |
| `i` | I2C scan |
| `b` | button levels for 10 s (prints changes) |
| `l` | receiver pin level / edge count for 5 s |
| `r` | receive dump for 15 s (protocol, address, command, raw durations) |
| `a` | battery-sense pin millivolts |
| `arm` | allows **one** emitter command within 30 s |
| `t [n]` | `n` (1-20) bounded 38 kHz bursts, 2 ms on / 8 ms off |
| `n` | one NEC frame (address 0x00, command 0x08) |
| `x` | **loopback**: receiver on, send one NEC frame, report what the receiver decoded |
| `g` | gate high for 3 s (DC measurements; only with the series resistor fitted) |

---

## B0. Preparation (no power)

- Print or open `docs/wiring.md` and this plan. Make a copy of the log template.
- Photograph each part and its markings (LED, MOSFET, modules, cell). Write down every marking.
- Check the workspace: non-conductive surface, no loose wire strands, cell away from the bench until B9.

**Pass:** you have the log, the tools and a photo/marking list. **If it fails:** do not continue.

## B1. Identify the parts (no power or multimeter only)

| Item | Do | Pass *(proposed)* | If it fails |
|---|---|---|---|
| Resistors | Measure each one out of circuit; compare with the colour code | Every resistor you will use has a measured value; use the measured value in all calculations | Do not use unmeasured resistors |
| IR LED polarity | Diode mode: red probe on the anode reads a forward voltage (roughly 1.0-1.3 V is typical for 940 nm IR LEDs, *yours may differ*); reversed reads open | Polarity known; Vf at 1 mA recorded for **each** LED; all LEDs within about 0.1 V of each other | If values differ a lot they are not all the same part: treat them separately |
| LED model | Marking, body colour, flat side, any datasheet | Model identified **or** declared unknown (then limit current as in 4.5) | Unknown is allowed; the rating limits then apply |
| MOSFET | Read the marking; pinout from the *manufacturer's* datasheet; diode mode between pins | G-D-S identified from the datasheet; no short between any two pins (the body diode shows only between source and drain) | A shorted part is discarded |
| TSOP38238 | Marking; pinout 1 = OUT, 2 = GND, 3 = VS from the Vishay datasheet package drawing | Pin 1 located with the drawing; marking matches TSOP38238 | If marking differs, use the real part's datasheet |
| 18650 cell | Marking, protected or not, body condition, voltage with the multimeter | 3.0-4.2 V, no damage; protected status known | Swollen / dented / hot / below 2.5 V: do not use |
| TP4056-class module | Read the chip markings (charger, DW01, 8205) and the LEDs/jumpers | Chips identified; B+/B- and OUT+/OUT- found | If there are no protection chips, treat it as unprotected |
| "XL6009" module | Read the main chip, note the trimmer, measure it unpowered | Chip marking recorded | Unknown chip: do not assume XL6009 behaviour |

## B2. ESP32 alone

Wiring: the board on USB only. Nothing on any pin.

1. Flash `bench`. Open the console: `m` prints the reset reason (`power on`) and the pin map.
2. Multimeter: 3V3 pin to GND reads about 3.3 V *(proposed 3.2-3.4 V)*; GPIO23 to GND reads about 0 V after boot.
3. Press and release the board's EN button: console resets; GPIO23 still about 0 V.

**Pass:** 3V3 within range, GPIO23 low at boot and after reset, serial works. **If it fails:** try another USB cable (data cable) / port; a wrong 3V3 means a defective board.

## B3. OLED

Wiring (4.2). Console `i`. Then flash `display_probe`.

1. `i` shows exactly one device at 0x3C (or 0x3D). Record it.
2. `display_probe` alternates SH1106 and SSD1306 pictures every 6 s. Record which picture is clean (full border touching all edges, clean diagonals, sharp text) and which is shifted/noisy.
3. Flash the matching firmware (`esp32dev` for SH1106, `esp32dev_ssd1306` for SSD1306). Check on the real panel: titles, accents (á é í ó ú ñ), the inverted ¡ ¿, 21 columns per line, 8 rows, footer messages not overlapping, *Invertir video* works.

**Pass:** one driver gives a clean picture; the Spanish text is legible. **If it fails:** no answer on I2C: power and SDA/SCL swapped? Both pictures wrong: record photos; do not guess a third driver. Decide here which `platformio.ini` environment is yours and write it in the log.

## B4. Buttons

Wiring (4.3). Console `b`, then the UI.

1. `b`: press each key; every key must show `1 -> 0` on press and `0 -> 1` on release, no other key reacting.
2. Boot with a key held (firmware): nothing triggers on its own until released.
3. UI *Configuración > Diagnóstico > Prueba de botones*: all five rows light, "Pulso" for taps, "Largo" for a hold of about 0.7 s, Up/Down repeat while held.
4. Tap each key 20 times quickly: no double events.

**Pass:** 5/5 keys correct, no ghost events *(proposed)*. **If it fails:** a key stuck at 0 means a short or a strapping pin problem; a key that never changes is open/miswired. Debounce timings are provisional (`DebounceConfig`): note any miss or double and adjust only after more observations.

## B5. Receiver only (emitter not connected)

Wiring (4.4), powered from 3V3. Console.

1. Multimeter: VS about 3.3 V; OUT idles near 3.3 V (high). Supply current about 0.3-0.5 mA if you measure it *(datasheet: 0.25-0.45 mA typical at 3.3 V)*.
2. `l` with no remote in view: edges about 0 and time-low about 0 % in normal room light. Record under fluorescent / LED / sunlight too.
3. `r` and press a key of your reference remote 1 m away, aimed: protocol and bits as expected; addresses/commands match the remote's documentation when you have it. Hold a key: a repeat code appears flagged `repeat=1`, not as a new command.
4. Raw durations: compare to the nominal timing of that protocol; deviations of a few tens of µs are expected (the receiver reshapes pulses). Record 3 captures.
5. UI: *Escuchar* (save a capture into a button) and *Aprender señal* into a button; reboot; the stored signal is still there and equals the capture (same protocol, address, command; RAW original present).
6. Range/angle: receive reliably at 1, 3, 5 m and 0 / 30 / 60° from the reference remote; record the success rate of 10 presses at each.
7. Sony remotes (if any): see the datasheet note in 4.4; record whether decoding is reliable.

**Pass:** idle high, quiet when no remote, correct decode of at least one known remote, learn/store/reboot consistent. **If it fails:** no edges: check VS/GND/OUT pin order against the datasheet (a swapped pair can destroy the part); constant edges: ripple or ambient noise, add R1/C1 (4.4) or move away from the display cable; wrong decode: log the raw data and compare with the protocol; do not edit the catalogue without the cross-check tests.

## B6. Emitter gate without any LED

Wiring: MOSFET + Rg + Rpd as in 4.5, but the LED replaced by a **resistor load** (a measured resistor of about 100 Ω rated 0.5 W or more, from the 5 V of the USB board to the drain: 5 V / 100 Ω = 50 mA). Scope on the drain if available.

1. Boot: drain stays at 5 V (MOSFET off), GPIO23 about 0 V. Press EN: still off. (The pull-down works.)
2. `arm`, `g`: for 3 s measure Vgs (about 3.3 V), Vds and the current through the load. Compute `Rds_on = Vds / I` and compare with the part's datasheet.
3. Repeat step 2 with each MOSFET you intend to use (all three IRFZ44N if they are candidates).
4. `arm`, `t 5`: scope on the drain: 2 ms packets of a square wave, period about 26.3 µs (38 kHz), duty about 33 % *(library default; proposed tolerance +/-5 % on frequency)*, 5 packets, then silence; the gate returns to 0 V.
5. `arm`, `n`: the NEC pattern: leader mark about 9 ms, space 4.5 ms, then 32 bit pairs. Measure the leader length.
6. Feel the MOSFET after a few commands: not warm.

**Pass:** off at boot and reset; Vds *(proposed)* <= 0.2 V at the chosen load current; frequency within tolerance; no heating. **If a unit fails (Vds high or erratic):** that unit is not usable at 3.3 V gate drive; switch to a logic-level MOSFET that you checked in its datasheet (4.5) and repeat B6. **If the drain is never off:** missing pull-down or wrong pin.

### B6 with a multimeter only (no oscilloscope)

Steps 4-5 (waveform and 38 kHz) cannot be checked without a scope or logic analyser. Do the static part instead:

1. Unpowered, diode mode on each MOSFET: gate to the other two pins reads open in both directions; with the red probe on the
   source and the black on the drain the body diode reads about 0.4-0.7 V, reversed reads open. IRFZ44N TO-220, printed face
   towards you, legs down: **Gate, Drain, Source** left to right, the metal tab is the drain (secondary sources; confirm with
   the datasheet or the part's marking).
2. Use a load resistor of about 220 Ω (about 23 mA at 5 V, well inside a 0.25 W resistor): measure it first.
3. `arm`, `g` three times per MOSFET; in each run read one quantity with the meter already set to a fixed DC range:
   Vgs, then Vds, then the voltage across the load resistor (current = that voltage / the measured resistance).
   `Rds_on = Vds / I`.
4. After `t 20` and `n` the drain must be back at the supply level and the gate at 0 V, and the part must not be warm.
5. The carrier (38 kHz) and the NEC timing are verified indirectly in B7: the TSOP only answers inside its band-pass, so
   `x` (loopback) reporting `LOOPBACK OK` is evidence that the emitter chain produced a decodable 38 kHz NEC frame. It is
   not a measurement of the exact frequency.

## B7. LED with a current-limiting resistor

Wiring: the real LED and R_led instead of the dummy load, branch supplied from 5 V USB (or 3V3 at low current), **not from the battery**.

1. Choose R_led from 4.5 using the Vf you measured and a target of **<= 20 mA DC-equivalent** while the LED rating is unknown. Fit it before anything else.
2. Before powering: measure the resistor and check polarity again.
3. `arm`, `g`: measure the DC current with the multimeter in series (mA range) and Vf across the LED. Current must be at or below the target *(proposed: within +20 % of the calculation)*. Touch the LED and resistor after: only slightly warm.
4. `arm`, `t 5`: the phone camera sees the LED blink; a scope on the resistor shows the current pulses.
5. `arm`, `x` (loopback): aim the LED at the TSOP from 20-30 cm. Expect `LOOPBACK OK` (the receiver decodes the NEC frame as sent). If `PARTIAL`, record the printed fields. Repeat at 10, 30, 60, 100 cm and note the maximum distance with OK.
6. Only if the LED's datasheet or a verified equivalent is found: raise the current step by step, staying under half of its pulse rating, and update `TxSafety` limits in `IrTransmitterAdapter.h`.

**Pass:** DC current within the limit, LED not hot, loopback OK at 20-30 cm. **If it fails:** no light: LED reversed or open; weak: too much R, Vds too high, or the MOSFET is not saturated; loopback fails but the camera sees light: TSOP not aimed, or its supply is noisy.

## B8. Aim and range against real devices

Use the full firmware (`esp32dev` or `esp32dev_ssd1306`). No promise about range is made anywhere in this project: the result depends on LED, current, optics, aim and the target.

1. Learn 5 signals from a reference remote (power, volume, etc.). Send each from 1, 2, 3, 5 m at 0 / 15 / 30 / 45° to your target device: 10 tries per point. Compare with the original remote at the same points.
2. A signal that fails on the target but decodes correctly in the bench loopback points at carrier/duty or timing differences (e.g. 36 / 40 kHz protocols; Sony uses 40 kHz in the library); log protocol, address, command.
3. Add LED branches **one at a time**, each with its own resistor (4.5); after each one repeat B7 steps 3-5 and the supply check of B9. Record whether range or reliability improves.
4. Exploration (*Herramientas IR > Exploración*), only on your own device, with a small range (16 codes): confirm the pause is respected, *Izq* stops it immediately, the log screen lists what was sent.

**Pass:** the device responds to learned/manual signals at the distances you need, or the limiting factor is identified. **If it fails:** record it; do not raise currents without the LED's rating.

## B9. Power: battery, charger, regulation

Do this as separate sub-blocks. Attended, on a non-flammable surface. Skip any sub-block whose parts you have not identified in B1.

| ID | Check | Pass *(proposed)* |
|---|---|---|
| B9a | Cell in the holder (polarity checked), voltage with the multimeter; protected module B+/B- connected, **no load**: OUT+/OUT- voltage | OUT equals the cell voltage within a few tens of mV; module not warm |
| B9b | Regulation, **no ESP32 yet**: the boost / buck-boost module with a resistive load of about 100-150 mA at the output (e.g. 33 Ω at 5 V ~ 150 mA); sweep the input from 3.0 to 4.2 V with a bench supply or the cell | Output within about 5 % of the target over the whole input range, ripple small, module not hot. Record the minimum input at which the XL6009-class module still regulates (datasheet revisions disagree: 3.6 V vs 5 V) |
| B9c | ESP32 powered from the battery path (USB disconnected), UI running 30 min | No resets, no `BROWNOUT` in *Historial arranques*; record the supply current with the multimeter in series |
| B9d | Supply sag while transmitting: scope on the 3.3 V pin and on the LED-branch supply; run `t 20`, `n`, a full exploration of 16 codes | 3.3 V rail stays above 3.0 V *(proposed; check the module datasheet)*; 100 transmissions without a `BROWNOUT` boot entry |
| B9e | Charging **with the device switched off**: attended for the first 15 minutes, then periodic checks; measure the cell voltage at the end | Module and cell only mildly warm; the end-of-charge voltage is about 4.2 V and the charge terminates by itself (the module's LED changes) |
| B9f | Charging with the device ON | **Not allowed** with a plain TP4056-class module. Allowed only after adding a verified power-path stage, then repeat B9e and B9d while charging |
| B9g | Low battery behaviour | Do **not** deep-discharge a cell to test the protection. Rely on the protection module's datasheet |

**If a sub-block fails:** stop, disconnect the cell, log the symptom. A sag/brownout during B9d is solved by (in this order) shorter / thicker wires to the LED branch, a larger bulk capacitor, a healthier cell, a lower LED current, and only then by changing the regulator.

## B10. Integration and endurance

Full firmware on the final wiring.

1. Create 2 controls with 5 buttons each (learn 3, manual 2); reboot; everything is present. Send each button.
2. Storage power-cut test: create/edit/save about 20 times while removing power at random moments (switch off during a *Guardado* toast and at random). After each boot the library loads (current or backup index; the boot toast says which). Record any "Perdido" or "Datos dañados" message, with the number of cuts.
3. *Configuración > Diagnóstico > Estado*: note `Heap` (free and minimum) after boot and after a long session. If the minimum free heap is below about 60 KB, report it: the memory budget assumptions of `docs/persistence.md` need revisiting.
4. Endurance: 1 hour with the screen on, 100 sends, 2 captures every 10 minutes: no unexpected resets, no watchdog entry, no display glitches.
5. Thermal: nothing noticeably hot (MOSFET, resistor, regulator, cell) after the endurance run.

**Pass:** no data loss across the power-cut series, no resets, heap healthy. **If it fails:** log the reset reason and the boot history; reproduce with the `bench` console before changing firmware.

## B11. Acceptance

The first hardware version is accepted when all of these are ticked, each with its log entry:

- [ ] OLED driver identified and the matching environment selected (B3)
- [ ] 5/5 buttons correct, no ghost events (B4)
- [ ] Receiver decodes at least one known remote; learn / store / reboot consistent (B5)
- [ ] Gate off at boot and reset; carrier near 38 kHz; MOSFET drop within the criterion (B6)
- [ ] LED current known and within the safe limit; loopback OK (B7)
- [ ] At least one real target device is controlled by a learned signal (B8)
- [ ] Battery path validated or explicitly deferred (B9a-B9e)
- [ ] No brownout across 100 transmissions (B9d)
- [ ] Power-cut series without data loss (B10)
- [ ] The values you learned are written back: `TxSafety`, LED current, the selected display environment, the debounce
      timings if changed, the battery calibration if the divider is built (and a real `IBatteryMonitor` is then added)

Anything unticked stays listed under "Pending" in `docs/risks.md`.

## What the results feed back into the project

| Result | Where it goes |
|---|---|
| Real LED rating and current | `TxSafety` in `src/infrastructure/esp32/IrTransmitterAdapter.h`, `docs/ir-notes.md` |
| Display controller | which `platformio.ini` environment is the default |
| Button timing observations | `DebounceConfig` in `lib/iris_core/src/ui/ButtonDebouncer.h` |
| Receiver noise / R1-C1 values | `docs/wiring.md` 4.4 |
| Power findings (sag, regulator choice) | `docs/wiring.md` 4.7, `docs/risks.md` |
| Heap numbers | `docs/persistence.md`, limits in `domain/Limits.h` |
| Battery calibration | a new `IBatteryMonitor` implementation (not written yet) |
