#pragma once

#include <memory>

#include "ui/Ui.h"

namespace iris {

// The home screen (battery state, controls, learn, create, IR tools, settings). Everything else is
// reached from here. All UI text is Spanish; identifiers and comments stay English.
std::unique_ptr<Screen> makeHomeScreen();

}  // namespace iris
