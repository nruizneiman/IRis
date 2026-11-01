// Central place for every capacity limit. PROVISIONAL values: they protect RAM and flash and
// must be re-evaluated with measurements on real hardware (see docs/risks.md).
#pragma once

#include <cstddef>
#include <cstdint>

namespace iris::limits {

constexpr size_t kMaxCategories = 16;
constexpr size_t kMaxRemotes = 32;
constexpr size_t kMaxButtonsPerRemote = 40;
constexpr size_t kMaxNameBytes = 20;  // UTF-8 bytes, not characters

constexpr size_t kMaxRawEntries = 1024;         // per raw frame (marks + spaces)
constexpr size_t kMaxFramesPerList = 8;          // initial / repeat frame lists
constexpr size_t kMaxStateBytes = 64;            // state-based (A/C style) payload
constexpr size_t kMaxTotalRawEntries = 20000;    // across the whole library
constexpr size_t kMaxRawEntriesPerRemote = 4000; // bounds the RAM needed to (de)serialize one remote file
constexpr uint8_t kMaxRepeatCount = 20;
constexpr uint32_t kMaxRepeatGapUs = 500000;

constexpr uint16_t kMinRawDurationUs = 1;        // 0 is never a valid duration
constexpr uint32_t kMinCarrierHz = 20000;
constexpr uint32_t kMaxCarrierHz = 60000;

// Exploration mode: hard caps that no UI setting can exceed.
constexpr uint32_t kExplorationHardMaxTransmissions = 1000;
constexpr uint32_t kExplorationConfirmThreshold = 50;  // above this, explicit confirmation
constexpr uint32_t kExplorationMinGapMs = 250;
constexpr uint32_t kExplorationMaxGapMs = 60000;

}  // namespace iris::limits
