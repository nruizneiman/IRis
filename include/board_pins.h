// Single source of truth for GPIO assignment. NOTHING else in the code base may contain GPIO numbers.
//
// STATUS: PROPOSED, NOT VERIFIED on the physical board. Check against the silkscreen of your
// 30-pin ESP32-WROOM-32 board before wiring (pin sets vary between 30-pin variants).
//
// Rules used:
//   - GPIO 6..11 are wired to the SPI flash: never use.
//   - GPIO 0, 2, 5, 12, 15 are strapping pins: avoided (GPIO 12 held high at boot selects 1.8 V flash).
//   - GPIO 1, 3 are the USB-serial UART0: avoided.
//   - GPIO 34..39 are input-only and have no internal pull-up/down.
#pragma once

#include <stdint.h>

namespace board {

// I2C for the OLED (ESP32 default pins)
constexpr uint8_t kI2cSda = 21;
constexpr uint8_t kI2cScl = 22;

// IR receivers (TSOP38238 output has its own pull-up; input-only pins are fine)
constexpr uint8_t kIrRx1 = 34;
constexpr uint8_t kIrRx2 = 35;  // second receiver, enabled later

// IR emitter gate drive (to the logic-level MOSFET gate through a series resistor, with a pull-down)
constexpr uint8_t kIrTx1 = 23;
constexpr uint8_t kIrTx2 = 19;  // reserved for a second emitter channel

// Buttons, wired GPIO -> button -> GND, internal pull-up, active low
constexpr uint8_t kBtnUp = 13;
constexpr uint8_t kBtnDown = 14;
constexpr uint8_t kBtnLeft = 27;
constexpr uint8_t kBtnRight = 26;
constexpr uint8_t kBtnOk = 25;

// microSD module (SPI, own pin set because the ESP32 default VSPI pins 23/19 are taken by the IR emitters).
// PROPOSED, NOT VERIFIED on the board: all four are free GPIOs, none is a strapping or flash pin.
constexpr uint8_t kSdSck = 18;
constexpr uint8_t kSdMiso = 16;
constexpr uint8_t kSdMosi = 17;
constexpr uint8_t kSdCs = 4;

// Battery voltage divider (ADC1). Only used when the circuit exists; otherwise "not available".
constexpr uint8_t kBatterySense = 33;

}  // namespace board
