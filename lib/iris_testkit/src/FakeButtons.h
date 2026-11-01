#pragma once

#include "hardware/IButtonInput.h"

namespace iris::testkit {

class FakeButtons : public IButtonInput {
 public:
  bool isPressed(Key key) const override { return level_[static_cast<unsigned>(key)]; }
  void set(Key key, bool pressed) { level_[static_cast<unsigned>(key)] = pressed; }

 private:
  bool level_[kKeyCount] = {};
};

}  // namespace iris::testkit
