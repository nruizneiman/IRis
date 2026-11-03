# Assembly plan: soldered boards (draft, nothing soldered yet)

Goal: move the bench wiring from the breadboard to two perfboards (7 x 9, hole counts or cm to confirm) so that
parts are not disturbed by accident. This is a plan to review **before** soldering; it uses the pins in
`include/board_pins.h`. The OLED, the receiver, the emitter on GPIO23 and the SD module have worked on the bench; the five
buttons have been used through the UI, but their per-key checks (test B4) are not closed.

## Rules
1. Solder only what is validated. Pending: emitter driver (NPN or logic-level MOSFET), battery divider on GPIO33
   and its calibration, SD module on battery power. Those get a connector, not a permanent joint.
2. ESP32 on **female headers** (to be bought; 2 x 15 pins, 2.54 mm; check the row spacing of the actual board).
3. One connector per off-board part (2.54 mm pin headers, plugged with the Dupont cables already at hand).
4. One ground point (star): battery -, boost -, ESP32 GND, emitter transistor emitter and bulk capacitors meet there.

## Board split
- **Front board**: the five 6x6 tactile switches, one pin of each to a common GND line, the other to its own pad.
  One 6-pin connector (GND + 5 signals) to the main board. The OLED is screwed to the case and uses its own cable.
- **Main board**: ESP32 on sockets, the emitter driver, the battery divider, the bulk capacitor and the connectors below.
  The charger, the XL6009, the 18650 holder and the microSD module stay as separate modules (screwed in the case)
  and are wired to the main board through connectors.

## Connectors on the main board (pin order = left to right, to be re-checked against each module's silkscreen)

| Connector | Pins | Goes to |
|---|---|---|
| J_OLED (4) | GND, 3V3, SCL=GPIO22, SDA=GPIO21 | OLED module. Its order varies by module: read the labels |
| J_BTN (6) | GND, GPIO13 Up, GPIO14 Down, GPIO27 Left, GPIO26 Right, GPIO25 OK | front board; buttons wired to GND, internal pull-ups |
| J_RX1 (3) | 3V3, GND, OUT=GPIO34 | TSOP38238 no. 1 (pinning 1=OUT, 2=GND, 3=VS: from the datasheet drawing) |
| J_RX2 (3) | 3V3, GND, OUT=GPIO35 | TSOP38238 no. 2 (firmware support is later) |
| J_SD (6) | GND, VCC=VIN (5 V), MISO=GPIO16, MOSI=GPIO17, SCK=GPIO18, CS=GPIO4 | microSD module (silkscreen order to confirm) |
| J_LED (2) | LED anode side, LED cathode side | IR LED(s) with their series resistor |
| J_PWR (2) | 5 V from the XL6009 output, GND | to the board's VIN and GND |
| J_BAT (1+GND) | switched battery (+) for the divider, GND | sense wire only; not the power path |

## Power path (as tested on the bench on 2026-11-02, see `docs/logs/2026-11-02-power.md`)
```
18650 holder ── B+ / B-  (charger)  ── OUT+ ─ [power switch] ─ XL6009 IN+      XL6009 OUT+ (5.0 V) ── VIN
                                       OUT- ───────────────── XL6009 IN-      XL6009 OUT- ────────── GND
USB-C of the charger: charging only, with the switch OFF.     USB of the ESP32: programming only, battery path unplugged.
```
- The power switch is not fitted yet; today the cell is removed from the holder to switch off.
- The divider (2 x 10 k) hangs from the switched battery side to GND; its midpoint goes to GPIO33.

## Emitter driver (replaces driving the LED straight from the GPIO)
Proposal with a general-purpose NPN (2N2222 or BC337, **not yet bought**, datasheet gain to check):
```
VIN (5 V) ──[R_led]── LED anode ... LED cathode ── NPN collector
GPIO23 ──[1 kΩ]── NPN base          NPN emitter ── GND
                  [100 kΩ] base to GND (keeps it off while the GPIO floats at boot)
```
- Base current from 3.3 V is about (3.3 - 0.7) / 1 kΩ = 2.6 mA *(calculated)*, enough for tens of mA of collector current
  with a gain of 100 or more, *to confirm with the chosen part's datasheet*.
- `R_led` follows `docs/wiring.md` 4.5: e.g. 220 Ω from 5 V gives roughly 16 mA with Vf about 1.2 V *(assumed, not measured)*; the LED
  rating is unknown, so the first tests stay near 20 mA.

## Order of work
1. Buy: 2 x 15-pin female headers, male header strip, one NPN, 1 kΩ and 100 kΩ resistors, a 100 µF capacitor.
2. Measure the real board (length, width, pin row spacing) and the OLED / SD / charger / XL6009 outlines; then draw the perfboard layout
   (hole grid) and review it together.
3. Build the front board first (switches and common line), test it with the bench `b` command.
4. Build the main board in stages, testing after each connector.
