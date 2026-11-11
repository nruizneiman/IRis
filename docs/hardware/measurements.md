# Measurements on the author's unit

What was actually measured or observed while building the unit (bench sessions from 2026-10-31 to 2026-11-03). The
only instrument was a multimeter: currents are calculated from resistor values unless a row says otherwise, and
distances are estimates.

| Block | Result |
|---|---|
| Board | 30-pin ESP32-WROOM-32 dev board, CP210x USB-serial. The pins of `include/board_pins.h` were used as they are |
| OLED | 1.3" 128x64 I2C, **SSD1306**: with `display_probe` its test picture was clean and the SH1106 one was not. The menus read well on the panel |
| Receiver | TSOP38238 on GPIO34. NEC remotes are decoded and learned through the UI |
| Emitter driver | Two IRFZ44N MOSFETs did **not** switch on from the 3.3 V GPIO (gate at 3.2 V, drain barely moving). Replaced by a 2N2222: GPIO23 to 1 kΩ to the base, emitter to GND, collector to the LED cathodes |
| Emitter range, with a TV as the target | 1 LED with 220 Ω (about 16 mA): a few cm. 1 LED at about 32 mA: under 1 m. **2 LEDs, each with its own 100 Ω from 5 V (about 34 mA each): almost 2 m** |
| microSD | 6-pin SPI module powered from VIN (5 V), read at 4 MHz on SCK 18 / MISO 16 / MOSI 17 / CS 4. Card of 16 GB, FAT32 |
| Power | One 18650, TP4056-class USB-C charger with protection, XL6009 boost set to 5.00 V without load. With the cell at 3.87 V the board ran at 4.96 V on VIN |
| Battery reading | Divider of 2 x 10 kΩ to GPIO33 (both measured at about 10 kΩ): 3.8 V on screen against 3.77 V on the multimeter. One point only |

A lesson worth repeating: a loose breadboard contact on the emitter drive looked, for a whole session, like a range
problem. Check that the drive pin of the transistor really moves before judging the range.

## Not measured

- The LED current while sending, and the forward voltage of the LEDs.
- The supply while sending (sag, brownout margin). No oscilloscope was used.
- The carrier frequency and the timing of the frames. The evidence is indirect: the receiver and a TV decode them.
- The battery reading near an empty cell (3.3 V) and a full one (4.2 V).
- The behaviour with a nearly empty cell, and the voltage at which the boost converter stops.
- A full charge cycle through the USB-C charger.
- The per-key button checks of the bring-up plan (long press, repeat, 20 fast taps without double events) were not
  recorded one by one; the keys are used daily through the UI.

What these unknowns mean for someone building a unit is in [limitations](../limitations.md).
