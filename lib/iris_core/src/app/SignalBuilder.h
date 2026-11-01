// Manual creation of signals from user-entered fields. Always validates before returning.
#pragma once

#include <vector>

#include "Result.h"
#include "domain/IrSignal.h"

namespace iris {

// One decoded frame, sent once. No repeat policy is assumed: protocol-specific repeat frames
// are added later by the transmitter adapter or by editing the signal.
Result<IrSignal> buildDecodedSignal(Protocol protocol, uint32_t address, uint32_t command,
                                    uint16_t bits, bool toggle = false);

// RAW mark/space sequence. carrierHz == 0 keeps the carrier "unknown"; the transmitter then
// applies its configured default.
Result<IrSignal> buildRawSignal(const std::vector<uint16_t>& durationsUs, uint32_t carrierHz = 0);

}  // namespace iris
