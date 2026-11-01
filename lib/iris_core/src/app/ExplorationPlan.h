// Bounded code exploration. The plan only describes WHAT could be tried; it enforces hard caps.
// The device cannot know whether the target reacted: the emitter gives no feedback.
#pragma once

#include <cstdint>

#include "Result.h"
#include "domain/Protocol.h"

namespace iris {

struct ExplorationPlan {
  Protocol protocol = Protocol::Unknown;
  uint16_t bits = 0;
  uint32_t addressFirst = 0, addressLast = 0;
  uint32_t commandFirst = 0, commandLast = 0;
  uint32_t maxTransmissions = 0;  // user cap; never above limits::kExplorationHardMaxTransmissions
  uint32_t gapMs = 0;
};

struct ExplorationCandidate {
  uint32_t address = 0;
  uint32_t command = 0;
};

Status validateExplorationPlan(const ExplorationPlan& plan);
// Number of codes that would really be sent: min(range size, maxTransmissions). 0 if invalid.
uint32_t explorationCandidateCount(const ExplorationPlan& plan);
// True when the run is long enough that the UI must ask for explicit confirmation.
bool explorationNeedsConfirmation(const ExplorationPlan& plan);
// Address-major enumeration, each (address, command) pair exactly once.
Result<ExplorationCandidate> explorationCandidateAt(const ExplorationPlan& plan, uint32_t index);

}  // namespace iris
