#include "app/ExplorationPlan.h"

#include <algorithm>

#include "domain/Limits.h"

namespace iris {

namespace {
uint64_t rangeSize(const ExplorationPlan& p) {
  return static_cast<uint64_t>(p.addressLast - p.addressFirst + 1) *
         static_cast<uint64_t>(p.commandLast - p.commandFirst + 1);
}
}  // namespace

Status validateExplorationPlan(const ExplorationPlan& p) {
  const ProtocolInfo* info = findProtocol(p.protocol);
  if (!info) return Status::fail(Err::InvalidArgument, "unknown protocol");
  const ProtocolVariant* v = findVariant(*info, p.bits);
  if (!v) return Status::fail(Err::OutOfRange, "unsupported bit count");
  if (p.addressFirst > p.addressLast || p.commandFirst > p.commandLast) {
    return Status::fail(Err::InvalidArgument, "range start is after range end");
  }
  if (p.addressLast > maskForBits(v->addressBits) || p.commandLast > maskForBits(v->commandBits)) {
    return Status::fail(Err::OutOfRange, "range exceeds protocol field width");
  }
  if (p.maxTransmissions == 0) return Status::fail(Err::InvalidArgument, "max transmissions must be > 0");
  if (p.maxTransmissions > limits::kExplorationHardMaxTransmissions) {
    return Status::fail(Err::LimitReached, "max transmissions above hard limit");
  }
  if (p.gapMs < limits::kExplorationMinGapMs || p.gapMs > limits::kExplorationMaxGapMs) {
    return Status::fail(Err::OutOfRange, "gap out of allowed range");
  }
  return Status::success();
}

uint32_t explorationCandidateCount(const ExplorationPlan& p) {
  if (!validateExplorationPlan(p).ok()) return 0;
  return static_cast<uint32_t>(std::min<uint64_t>(rangeSize(p), p.maxTransmissions));
}

bool explorationNeedsConfirmation(const ExplorationPlan& p) {
  return explorationCandidateCount(p) > limits::kExplorationConfirmThreshold;
}

Result<ExplorationCandidate> explorationCandidateAt(const ExplorationPlan& p, uint32_t index) {
  Status st = validateExplorationPlan(p);
  if (!st.ok()) return st;
  if (index >= explorationCandidateCount(p)) return Status::fail(Err::OutOfRange, "index past end of plan");
  const uint64_t commands = static_cast<uint64_t>(p.commandLast - p.commandFirst) + 1;
  ExplorationCandidate c;
  c.address = p.addressFirst + static_cast<uint32_t>(index / commands);
  c.command = p.commandFirst + static_cast<uint32_t>(index % commands);
  return c;
}

}  // namespace iris
