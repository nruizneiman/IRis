// Reads the text format of Flipper-IRDB `.ir` files (CC0) and converts each signal into an IrSignal.
// Pure C++ (no file access): the caller supplies the text, so it runs on the PC and on the ESP32.
//
// Every protocol Flipper writes is converted. Field meaning confirmed by reading Flipper's own encoders
// (flipperzero-firmware, lib/infrared/encoder_decoder, read 2026-11-01 and 2026-11-12):
//   parsed NEC       8-bit address, 8-bit command (inverses implied)         -> Protocol::Nec
//   parsed NECext    16-bit address, 16-bit command whose high byte is the
//                    inverse of the low byte                                 -> Protocol::Nec
//   parsed Samsung32 8-bit address, 8-bit command                            -> Protocol::Samsung
//   parsed SIRC / SIRC15 / SIRC20                                            -> Protocol::Sony (12/15/20 bits)
//   parsed RC5 / RC5X  5-bit address, 6-bit command                          -> Protocol::Rc5 (12/13 bits)
//   parsed NECext (other commands), NEC42, NEC42ext, RC6, Kaseikyo, RCA,
//                    Pioneer                                                 -> RAW frame built by FlipperSynth.h
//   raw              frequency + mark/space list (<= 1024 entries)           -> RAW frame
// A protocol name that is none of these is counted as unsupported and reported, never guessed.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "Result.h"
#include "domain/IrSignal.h"
#include "domain/Limits.h"

namespace iris {

struct ImportedButton {
  std::string name;
  IrSignal signal;
};

struct ImportReport {
  size_t seen = 0;          // signal blocks found in the file
  size_t imported = 0;
  size_t unsupported = 0;   // a protocol or type name that Flipper does not define
  size_t invalid = 0;       // malformed or out-of-range values
  size_t overCapacity = 0;  // dropped because the remote button or RAW budget was full
  std::string firstProblem; // short text about the first skipped signal (for the UI / log)
};

struct ImportedRemote {
  std::vector<ImportedButton> buttons;
  ImportReport report;
};

// Fails (not ok) only when the file is not a Flipper IR file or has an unknown version. Individual bad signals are
// skipped and counted in the report.
Result<ImportedRemote> parseFlipperIr(std::string_view text, size_t maxButtons = limits::kMaxButtonsPerRemote,
                                      size_t rawBudget = limits::kMaxRawEntriesPerRemote);

}  // namespace iris
