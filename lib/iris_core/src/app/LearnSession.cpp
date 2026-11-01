#include "app/LearnSession.h"

#include "domain/Limits.h"
#include "domain/SignalValidation.h"

namespace iris {

void LearnSession::note(LogLevel level, const std::string& msg) const {
  if (log_) log_->log(level, "learn", msg);
}

Status LearnSession::start(uint32_t timeoutMs) {
  if (state_ == LearnState::Armed) return Status::fail(Err::Unavailable, "capture already in progress");
  if (timeoutMs == 0 || timeoutMs > kMaxTimeoutMs) return Status::fail(Err::OutOfRange, "timeout out of range");
  result_.reset();
  Status st = receiver_.start();
  if (!st.ok()) {
    finish(LearnState::Failed, st);
    return st;
  }
  startMs_ = clock_.nowMs();
  timeoutMs_ = timeoutMs;
  state_ = LearnState::Armed;
  status_ = Status::success();
  note(LogLevel::Info, "armed");
  return Status::success();
}

void LearnSession::cancel() {
  if (state_ == LearnState::Armed) receiver_.stop();
  result_.reset();
  state_ = LearnState::Idle;
  status_ = Status::success();
  note(LogLevel::Info, "cancelled");
}

void LearnSession::finish(LearnState next, Status status) {
  receiver_.stop();
  state_ = next;
  status_ = std::move(status);
}

void LearnSession::tick() {
  if (state_ != LearnState::Armed) return;
  if (auto capture = receiver_.poll()) {
    handleCapture(std::move(*capture));
    return;
  }
  if (clock_.nowMs() - startMs_ >= timeoutMs_) {
    finish(LearnState::TimedOut, Status::fail(Err::Unavailable, "no signal received in time"));
    note(LogLevel::Warn, "timed out");
  }
}

void LearnSession::handleCapture(ReceivedCapture capture) {
  CaptureAnalysis analysis = analyzeCapture(capture.raw.durationsUs);
  if (!analysis.status.ok()) {
    finish(LearnState::Rejected, analysis.status);
    note(LogLevel::Warn, "rejected: " + analysis.status.message);
    return;
  }

  QualityFlags quality = analysis.quality;
  if (capture.bufferOverflow) quality.truncated = true;
  if (capture.raw.durationsUs.size() > limits::kMaxRawEntries) {
    capture.raw.durationsUs.resize(limits::kMaxRawEntries);
    quality.truncated = true;
  }
  capture.raw.carrierHz = 0;  // a demodulating receiver cannot report the original carrier

  IrSignal s;
  s.provenance = Provenance::Learned;
  s.quality = quality;
  s.originalCapture = capture.raw;  // RAW is always kept, even for recognised protocols

  const bool flagged = quality.truncated || quality.possibleNoise;
  if (capture.decoded && validateDecodedFrame(*capture.decoded).ok()) {
    s.initial.push_back(Frame::makeDecoded(*capture.decoded));
    s.recognition = flagged ? Recognition::Partial : Recognition::Recognized;
  } else if (capture.decoded) {
    s.initial.push_back(Frame::makeRaw(capture.raw));
    s.recognition = Recognition::Partial;
    s.note = "protocol reported by decoder was rejected by validation";
  } else {
    s.initial.push_back(Frame::makeRaw(capture.raw));
    s.recognition = Recognition::Unknown;
  }

  Status st = validateSignal(s);
  if (!st.ok()) {
    finish(LearnState::Rejected, st);
    note(LogLevel::Warn, "rejected: " + st.message);
    return;
  }
  result_ = std::move(s);
  finish(LearnState::Captured, Status::success());
  note(LogLevel::Info, "captured");
}

std::optional<IrSignal> LearnSession::take() {
  if (state_ != LearnState::Captured) return std::nullopt;
  std::optional<IrSignal> out = std::move(result_);
  result_.reset();
  state_ = LearnState::Idle;
  return out;
}

}  // namespace iris
