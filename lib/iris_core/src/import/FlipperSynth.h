// Builds the mark/space list of a Flipper "parsed" signal whose protocol the firmware has no decoded form for, so the
// importer can keep it as a RAW frame instead of dropping it. Pure C++.
//
// Timings and bit layout were taken from Flipper's own encoders (flipperzero-firmware, lib/infrared/encoder_decoder,
// read 2026-11-12):
//   NECext            16-bit address, 16-bit command, sent as they are (no inverse bytes)
//   NEC42 / NEC42ext  13-bit address + inverse + 8-bit command + inverse / 26-bit address + 16-bit command
//   RC6               mode 0, 8-bit address, 8-bit command, 36 kHz
//   Kaseikyo          26-bit address (id, vendor, genre 1, genre 2), 10-bit command
//   RCA               4-bit address, 8-bit command
//   Pioneer           8-bit address, 8-bit command, 40 kHz, the frame sent twice
//   JVC               8-bit address, 8-bit command (not a Flipper protocol: used by the writer, from IRremoteESP8266)
// The RC6 toggle bit is always sent as 0: a RAW frame cannot alternate it.
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace iris {

enum class SynthResult { Ok, UnknownProtocol, OutOfRange };

// On Ok, `out` holds the durations (first one a mark, last one a mark) and `carrierHz` the carrier Flipper uses.
SynthResult synthesizeFlipperFrame(std::string_view protocol, uint32_t address, uint32_t command,
                                   std::vector<uint16_t>& out, uint32_t& carrierHz);

// JVC frame as IRremoteESP8266 sends it, for exporting a JVC button as RAW.
void synthesizeJvcFrame(uint8_t address, uint8_t command, std::vector<uint16_t>& out, uint32_t& carrierHz);

}  // namespace iris
