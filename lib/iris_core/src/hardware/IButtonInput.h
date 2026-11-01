#pragma once

#include <cstdint>

namespace iris {

enum class Key : uint8_t { Up, Down, Left, Right, Ok };
constexpr unsigned kKeyCount = 5;

// Raw, un-debounced level of each key. Debouncing and long-press detection live in the core
// (Stage 4) so they can be unit tested with a fake clock.
class IButtonInput {
 public:
  virtual ~IButtonInput() = default;
  virtual bool isPressed(Key key) const = 0;
};

}  // namespace iris
