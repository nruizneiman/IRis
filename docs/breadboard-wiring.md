# Breadboard wiring record (as of 2026-11-03)

Purpose: freeze exactly how the bench circuit was wired before the breadboard is taken apart and the real
(perfboard) build starts. Pin numbers come from `include/board_pins.h`; the circuit facts come from the builder's
bench notes. Nothing here was re-measured at the time of writing. Items marked **(unconfirmed)** were not recorded.

Related: `docs/assembly.md` (plan for the soldered boards), `docs/wiring.md` (design guidance),
`docs/hardware.md` (what was measured on the bench).

## 1. ESP32 (30-pin WROOM-32 dev board, USB-C)

| ESP32 pin | Goes to |
|---|---|
| VIN | XL6009 `OUT+` (5.0 V), through the power switch on the bench build (see 8) |
| GND | XL6009 `OUT-`, and the common ground of everything below |
| 3V3 | OLED `VCC`, TSOP38238 `VS` |
| GPIO21 | OLED `SDA` |
| GPIO22 | OLED `SCL` |
| GPIO13 / 14 / 27 / 26 / 25 | Buttons Up / Down / Left / Right / OK |
| GPIO34 | TSOP38238 no. 1 `OUT` |
| GPIO35 | not connected (receiver 2 not fitted) |
| GPIO23 | emitter driver: 1 kΩ to the NPN base |
| GPIO19 | not connected (reserved for a second emitter channel) |
| GPIO18 / 16 / 17 / 4 | microSD `SCK` / `MISO` / `MOSI` / `CS` |
| GPIO33 | battery divider midpoint |
| USB | programming only, with the battery path off (no battery and USB at the same time) |

## 2. OLED 1.3" 128x64, I2C (SSD1306 build)
- `VCC` -> ESP32 3V3
- `GND` -> ESP32 GND
- `SDA` -> GPIO21
- `SCL` -> GPIO22
- Pin order differs between modules: read the silkscreen of the actual module.

## 3. Buttons (five 6x6 tactile switches)
- One pin of each button to its own GPIO: Up 13, Down 14, Left 27, Right 26, OK 25.
- The other pin of each button to GND. No external resistors (internal pull-ups, active low).
- Decision for the real board: the five buttons share **one** GND line (common rail on the front board), one wire to
  the ESP32 GND. Each GPIO keeps its own wire. A 6x6 switch has pairs of legs that are joined inside: use one leg of
  the GND side and one of the signal side (check with continuity if unsure).
- Per-key checks of test B4 (key mapping, long press, repeat, 20 taps without double events) were not recorded
  individually.

## 4. IR receiver TSOP38238 no. 1
Dome facing you, legs down, left to right: 1, 2, 3.
- Pin 1 `OUT` -> GPIO34
- Pin 2 `GND` -> GND
- Pin 3 `VS` -> 3V3
- No R-C filter on the supply (none recorded).

## 5. microSD module (6 pins, level shifter + 1117 regulator on board)
- `VCC` -> VIN (5 V)
- `GND` -> GND
- `SCK` -> GPIO18
- `MISO` -> GPIO16
- `MOSI` -> GPIO17
- `CS` -> GPIO4
- Card: 16 GB, FAT32 (formatted from the ESP32), Flipper-IRDB files loaded.

## 6. IR emitter driver (NPN 2N2222, plastic TO-92)
Pins seen from the flat face, left to right: **E, B, C** (from the curved face: C, B, E).

```
VIN (5 V) --[100 ohm]-- LED1 anode ... LED1 cathode --+
VIN (5 V) --[100 ohm]-- LED2 anode ... LED2 cathode --+-- collector
GPIO23 --[1 kohm]-- base --+                          emitter -- GND
                       [100 kohm] base to GND
```

- GPIO23 -> 1 kΩ -> base.
- 100 kΩ from base to GND: **fitted** (confirmed on 2026-11-03).
- Emitter -> GND.
- Collector -> cathodes of both LEDs.
- Each LED anode has its **own** 100 Ω resistor to VIN. Two LEDs in parallel branches.
- Calculated peak current about 34 mA per LED, 68 mA total (Vf about 1.4 V and Vce about 0.2 V assumed, not measured).
- Range seen with this configuration: TV responds at almost 2 m (no instrument, by eye). Earlier
  configurations and results are in `docs/hardware.md`.
- LED model: TSAL6200 (940 nm, 5 mm, 100 mA continuous rating per its datasheet). The forward voltage on this unit was not measured.

## 7. Battery voltage divider
- XL6009 `IN+` -> 10 kΩ -> midpoint -> 10 kΩ -> GND.
- Midpoint -> GPIO33.
- Both resistors measured about 10 kΩ out of circuit.
- 100 nF from the midpoint to GND: **fitted**.
- Firmware: `-DIRIS_BATTERY_RATIO_PERMILLE=2000`, set in the `esp32dev_ssd1306` environment.
- Calibration: one comparison only (3.8 V on screen against 3.77 V on the multimeter). Compare again near 3.3 V and 4.2 V.

## 8. Power chain
Parts: 18650 cell in a holder, TP4056-class USB-C charger module with protection, XL6009 boost module, a
2-pin on/off switch.

Bench build (as wired on the breadboard):
```
18650 --B+/B-- charger --OUT+/OUT-- XL6009 IN+/IN-
XL6009 OUT+ --[switch]-- ESP32 VIN
XL6009 OUT- ------------ ESP32 GND
```
- The switch is a plain 2-pin on/off switch **between XL6009 `OUT+` and `VIN`**.
- Consequence: the XL6009 and the divider stay connected to the cell when the switch is off (the divider alone draws about 190 µA).

Decision for the real board (decided before the build):
```
18650 --B+/B-- charger
charger OUT+ --[switch]-- XL6009 IN+ (+ divider)
charger OUT- ------------ XL6009 IN-
XL6009 OUT+ -- VIN        XL6009 OUT- -- GND
```
- The switch moves to **before** the XL6009 (charger `OUT+` to switch pin A, pin B to XL6009 `IN+`).
- The divider stays on XL6009 `IN+`, which is now the switched side, so it no longer drains the cell when off.
- Side effect: with the switch off and USB connected, GPIO33 reads about 0 V (empty battery bar). Not a fault.
- The switch must carry the battery-side current, which is higher than the 5 V side.
- XL6009 output set to 5.00 V unloaded with the trimmer, with the ESP32 disconnected. The board reads 4.96 V on `VIN` when running on battery.
- Charge only with the switch off. There is no power-path, so do not run the device from the module while it charges.

## 9. Not on the bench build
- TSOP38238 no. 2 (GPIO35).
- Second emitter channel (GPIO19).
- **100 µF bulk capacitor: fitted, but its exact position was not given (unconfirmed).** Record the position when
  rebuilding (planned: across `VIN`/`GND` near the ESP32, and one near the LED branch).

## 10. Carry over to the real board
1. Ground: star point where battery -, XL6009 -, ESP32 GND, NPN emitter and bulk capacitors meet.
2. Connectors as in `docs/assembly.md`: J_OLED, J_BTN (GND + 5 signals), J_RX1, J_SD, J_LED, J_PWR, J_BAT.
3. Keep the 100 kΩ base pull-down on the main board (it is on the bench build already).
4. Switch position: before the XL6009 (section 8).
5. Pending measurements: LED current while sending, supply sag during transmission, USB-C charge test, divider
   calibration near 3.3 V and 4.2 V.
