// Turns raw button levels into clean events: debounced press, long press, auto-repeat (Up/Down).
// Up/Down: Press on touch + Repeat while held. Other keys: Press on release (short), or one
// LongPress while held (then nothing on release).
// Keys already held when the first poll happens are ignored until released (no phantom presses
// after boot or a wake-up).
#pragma once

#include <vector>

#include "hardware/IButtonInput.h"
#include "hardware/IClock.h"

namespace iris {

enum class KeyAction : uint8_t { Press, LongPress, Repeat };

struct KeyEvent {
  Key key = Key::Ok;
  KeyAction action = KeyAction::Press;
};

struct DebounceConfig {
  uint32_t debounceMs = 30;
  uint32_t longPressMs = 700;
  uint32_t repeatDelayMs = 450;
  uint32_t repeatIntervalMs = 140;
};

class ButtonDebouncer {
 public:
  ButtonDebouncer(const IButtonInput& input, const IClock& clock, DebounceConfig cfg = {})
      : input_(input), clock_(clock), cfg_(cfg) {}

  // Appends the events produced since the last call. Call often (every loop iteration).
  void poll(std::vector<KeyEvent>& out);

 private:
  struct State {
    bool stable = false;       // debounced level (true = pressed)
    bool candidate = false;    // last raw level seen
    uint32_t candidateSince = 0;
    uint32_t pressedAt = 0;
    uint32_t nextRepeat = 0;
    bool longFired = false;
    bool blocked = false;      // held since startup, wait for release
  };

  const IButtonInput& input_;
  const IClock& clock_;
  DebounceConfig cfg_;
  State state_[kKeyCount];
  bool first_ = true;
};

}  // namespace iris
