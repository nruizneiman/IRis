# Bill of materials

Everything needed to build one IRis unit, as used in the author's unit. How the parts connect is in
[wiring](wiring.md); the case is described in [case](case.md) and published on
[MakerWorld](https://makerworld.com/en/models/3592563-iris-universal-ir-remote-case-esp32-oled).

## Electronics

| Qty | Part | Notes |
|---|---|---|
| 1 | ESP32-WROOM-32 dev board, 30 pins, micro-USB | CP210x USB-serial. Pin sets vary between 30-pin boards: check yours against `include/board_pins.h` |
| 1 | OLED 1.3" 128x64, I2C | SSD1306 (the author's) or SH1106; there is a firmware build for each ([how to tell](bring-up.md#identify-the-oled-controller)) |
| 1 | TSOP38238 | 38 kHz IR receiver |
| 2 | IR LED TSAL6200, 940 nm, 5 mm | Rated for 100 mA continuous. About 34 mA each was calculated, not measured |
| 1 | NPN transistor 2N2222 (TO-92) | Drives the LEDs from 5 V. An IRFZ44N did not switch on from a 3.3 V GPIO |
| 5 | Tactile switch, 6 x 6 mm | Up, down, left, right, OK |
| 1 | microSD module, SPI, 6 pins | Optional: only for the card features |
| 1 | microSD card, FAT | Optional |
| 1 | 18650 cell | |
| 1 | 18650 holder | The case is sized for 76 x 21.2 x 19.7 mm |
| 1 | USB-C charger module with protection (TP4056 class) | About 27.8 x 17.4 mm. No power path: charge with the device off |
| 1 | XL6009 boost converter | Set to 5.0 V before connecting the ESP32 |
| 1 | Rocker switch | Window in the case 8.3 x 13.1 mm. Goes between the charger and the XL6009 |

## Passive components

| Qty | Part | Notes |
|---|---|---|
| 2 | Resistor 100 ohm | One per LED, from 5 V |
| 1 | Resistor 1 kohm | GPIO23 to the transistor base |
| 1 | Resistor 100 kohm | Base to GND (pull-down) |
| 2 | Resistor 10 kohm | Battery divider. Optional: without it the screen shows `Bat n/a` |
| 1 | Capacitor 100 nF | Divider midpoint to GND. Optional, with the divider |
| 1 | Electrolytic capacitor 100 uF | Across the 5 V rail on the component board. Mind the polarity |

## Assembly

| Qty | Part | Notes |
|---|---|---|
| 1 | Perfboard for the components | Cut to 45.9 x 28.8 mm, 18 x 11 holes ([component board](component-board.md)) |
| 1 | Perfboard for the buttons | Cut to 23.7 x 23.7 mm |
| 2 | Female pin header, 1 x 15, 2.54 mm | Sockets for the ESP32 board |
| - | Hook-up wire | Soldered between the modules and the component board |
| - | Solder, heat-shrink tubing | |

## Screws (M3, threaded into PLA, 2.5 mm pilot holes)

| Qty | Use | Length |
|---|---|---|
| 4 | ESP32 | M3 x 6 |
| 4 | Button board | M3 x 6 |
| 4 | Component board | M3 x 5 |
| 2 | XL6009 | M3 x 5 |
| 4 | OLED | M3 x 4, or M3 x 5 with the printed 1 mm washers |

The holes of the modules are 3 mm: ream them to 3.2 mm. The microSD module takes no screws; two printed ribs hold it.
The counts come from the screw posts of the model: check them against your own print.

## Printed parts

| Qty | Part | Notes |
|---|---|---|
| 1 | Base | Floor on the bed |
| 1 | Cover | Front face on the bed |
| 4 | Direction keys | Flange on the bed |
| 1 | OK key | Flange on the bed |
| 4 | OLED washers | Optional, 100 % infill |

PLA, 0.4 mm nozzle, 0.2 mm layers, no supports. Settings and the reasoning: [case](case.md).

## Not recorded

The exact models of the rocker switch and the charger module were not written down. Any part with the same
electrical role and size should do. Check the LED current before changing LEDs or resistors
([measurements](measurements.md)).
