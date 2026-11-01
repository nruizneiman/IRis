#include "ui/ButtonDebouncer.h"

namespace iris {

void ButtonDebouncer::poll(std::vector<KeyEvent>& out) {
  const uint32_t now = clock_.nowMs();
  for (unsigned i = 0; i < kKeyCount; ++i) {
    const Key key = static_cast<Key>(i);
    State& s = state_[i];
    const bool raw = input_.isPressed(key);
    // Up/Down act immediately and auto-repeat (scrolling, value editing). The other keys emit
    // Press on release, or LongPress while still held, never both: a press that turns into a
    // long press must not also trigger its short-press action.
    const bool repeats = key == Key::Up || key == Key::Down;

    if (first_ && raw) s.blocked = true;
    if (s.blocked) {
      if (!raw) {  // released: start from a clean state
        s = State{};
        s.candidateSince = now;
      }
      continue;
    }

    if (raw != s.candidate) {
      s.candidate = raw;
      s.candidateSince = now;
    } else if (s.candidate != s.stable && now - s.candidateSince >= cfg_.debounceMs) {
      s.stable = s.candidate;
      if (s.stable) {
        s.pressedAt = now;
        s.longFired = false;
        s.nextRepeat = now + cfg_.repeatDelayMs;
        if (repeats) out.push_back(KeyEvent{key, KeyAction::Press});
      } else if (!repeats && !s.longFired) {
        out.push_back(KeyEvent{key, KeyAction::Press});
      }
    }

    if (s.stable) {
      const uint32_t held = now - s.pressedAt;
      if (!repeats && !s.longFired && held >= cfg_.longPressMs) {
        s.longFired = true;
        out.push_back(KeyEvent{key, KeyAction::LongPress});
      }
      if (repeats && static_cast<int32_t>(now - s.nextRepeat) >= 0) {
        s.nextRepeat = now + cfg_.repeatIntervalMs;
        out.push_back(KeyEvent{key, KeyAction::Repeat});
      }
    }
  }
  first_ = false;
}

}  // namespace iris
