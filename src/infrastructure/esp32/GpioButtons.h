#pragma once

#include <Arduino.h>

#include "board_pins.h"
#include "hardware/IButtonInput.h"

// Buttons wired GPIO -> switch -> GND with the internal pull-up (active low).
class GpioButtons : public iris::IButtonInput {
 public:
  void begin() {
    for (uint8_t pin : kPins) pinMode(pin, INPUT_PULLUP);
  }
  bool isPressed(iris::Key key) const override { return digitalRead(kPins[static_cast<unsigned>(key)]) == LOW; }

 private:
  // Order must match iris::Key: Up, Down, Left, Right, Ok
  static constexpr uint8_t kPins[iris::kKeyCount] = {board::kBtnUp, board::kBtnDown, board::kBtnLeft, board::kBtnRight,
                                                     board::kBtnOk};
};
