#include "app/SequenceRunner.h"

#include <algorithm>

namespace iris {

Status SequenceRunner::start(std::unique_ptr<ISignalSequence> sequence, uint32_t gapMs, uint32_t maxSends) {
  if (state_ == SequenceState::Running) return Status::fail(Err::Unavailable, "a sequence is already running");
  if (!sequence) return Status::fail(Err::InvalidArgument, "no sequence");
  sequence_ = std::move(sequence);
  gapMs_ = std::max(gapMs, kMinGapMs);
  maxSends_ = maxSends;
  sent_ = skipped_ = 0;
  last_.clear();
  inFlight_ = false;
  nextAtMs_ = clock_.nowMs();
  state_ = SequenceState::Running;
  if (log_) log_->log(LogLevel::Info, "sequence", "start gap " + std::to_string(gapMs_) + " ms");
  return Status::success();
}

void SequenceRunner::stop() {
  if (state_ != SequenceState::Running) return;
  tx_.cancel();
  state_ = SequenceState::Stopped;
  if (log_) log_->log(LogLevel::Info, "sequence", "stopped after " + std::to_string(sent_) + " sends");
}

void SequenceRunner::reset() {
  if (state_ == SequenceState::Running) return;
  sequence_.reset();
  state_ = SequenceState::Idle;
  sent_ = skipped_ = 0;
  last_.clear();
}

void SequenceRunner::service() {
  if (state_ != SequenceState::Running) return;
  const uint32_t now = clock_.nowMs();
  if (inFlight_) {
    if (tx_.busy()) return;
    inFlight_ = false;
    const uint32_t gap = sequence_ ? sequence_->gapAfterMs(gapMs_) : gapMs_;
    nextAtMs_ = now + std::max(gap, kMinGapMs);  // the pause counts from the end of the transmission
    return;
  }
  if (static_cast<int32_t>(now - nextAtMs_) < 0) return;
  if (maxSends_ != 0 && sent_ >= maxSends_) {
    state_ = SequenceState::Done;
    return;
  }
  if (tx_.busy()) return;
  SequenceItem item;
  if (!sequence_ || !sequence_->next(item)) {
    state_ = SequenceState::Done;
    if (log_) log_->log(LogLevel::Info, "sequence", "done after " + std::to_string(sent_) + " sends");
    return;
  }
  const Status st = tx_.send(item.signal);
  if (st.ok()) {
    ++sent_;
    last_ = item.label;
    inFlight_ = true;
  } else if (st.code == Err::Unavailable) {
    // Busy at this very moment: the item is lost, not retried (retrying could repeat a send the user cannot see).
    ++skipped_;
    nextAtMs_ = now + gapMs_;
  } else {
    ++skipped_;
    nextAtMs_ = now;  // invalid signal: go straight to the next one
  }
}

}  // namespace iris
