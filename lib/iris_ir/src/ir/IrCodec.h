// Mapping between the app's domain model and IRremoteESP8266 2.9.0. The only place that knows
// the library's data layout. Compiles for the ESP32 and (with -DUNIT_TEST and the library sources)
// on the PC, where test/hostlib cross-checks every protocol by encoding with IRsendTest and
// decoding with IRrecv. Those checks prove the mapping is consistent with the library; they say
// nothing about real receivers or emitters.
#pragma once

#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "domain/IrSignal.h"
#include "hardware/IIrReceiver.h"

namespace iris {

struct LibFrame {
  decode_type_t type = UNKNOWN;
  uint64_t data = 0;
  uint16_t nbits = 0;
  uint16_t repeat = 0;  // IRsend::minRepeats(type): protocols that need N extra copies to be accepted
};

// nullopt when the frame does not validate or has no library equivalent. `sender` is only used for
// its encode helpers (encodeNEC, encodeSony, ...), which are not static.
std::optional<LibFrame> toLibraryFrame(IRsend& sender, const DecodedFrame& frame);

// nullopt for repeat codes, buffer overflows, unsupported protocols and values the library
// reports outside the catalogue.
std::optional<DecodedFrame> fromLibraryResult(const decode_results& results);

// Raw mark/space durations in microseconds, first entry a mark. rawbuf[0] (the idle gap before the
// message) is dropped; values are clamped to 65535.
std::vector<uint16_t> rawDurationsUs(const decode_results& results);

// Builds the capture handed to the app. nullopt for pure repeat codes (a held key) and empty buffers.
std::optional<ReceivedCapture> captureFromResult(const decode_results& results, uint8_t receiverIndex,
                                                 uint32_t timestampMs);

}  // namespace iris
