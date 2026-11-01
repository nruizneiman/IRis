// Persistent data model: categories contain remotes, remotes contain buttons, buttons own a signal.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "domain/IrSignal.h"

namespace iris {

using Id = uint32_t;  // 0 is never a valid id

struct Category {
  Id id = 0;
  std::string name;
};

struct Button {
  Id id = 0;
  std::string name;
  std::optional<IrSignal> signal;  // empty until a signal is learned or created
};

struct Remote {
  Id id = 0;
  std::string name;
  Id categoryId = 0;
  std::vector<Button> buttons;
};

struct LibraryData {
  Id nextId = 1;
  std::vector<Category> categories;
  std::vector<Remote> remotes;
};

}  // namespace iris
