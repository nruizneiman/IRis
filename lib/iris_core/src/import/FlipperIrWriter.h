// Writes buttons in the Flipper-IRDB `.ir` text format, the same one FlipperIr.h reads, so an exported control can be
// imported again here or used on a Flipper. Mirrors the importer's field mapping.
//
//   Nec (address <= 0xFF)    -> NEC       Nec (16-bit address) -> NECext (command as value + inverse)
//   Samsung                  -> Samsung32
//   Sony 12 / 15 / 20 bits   -> SIRC / SIRC15 / SIRC20
//   Rc5 12 / 13 bits         -> RC5 / RC5X (the toggle bit is not part of the format)
//   Jvc                      -> raw (Flipper has no JVC protocol)
//   a RAW first frame        -> raw (carrier 38000 Hz when the capture did not record one)
// A decoded signal with values the format cannot hold is not written and is counted.
#pragma once

#include <string>
#include <vector>

#include "app/RemoteView.h"

namespace iris {

struct WrittenIr {
  std::string text;
  size_t written = 0;
  size_t skipped = 0;  // buttons without a signal or with one the format cannot carry
};

WrittenIr writeFlipperIr(const std::vector<ViewButton>& buttons);

}  // namespace iris
