#pragma once

#include <vector>

#include "hardware/IClock.h"
#include "hardware/IIrTransmitter.h"

namespace iris::testkit {

// Records what the app tried to send and models transmission TIME against a clock, so busy states,
// cancellation between frames and gaps can be tested. It produces no waveform.
// kAssumedDecodedFrameUs is a placeholder: real frame length depends on protocol and bit values.
class FakeIrTransmitter : public IIrTransmitter {
 public:
  static constexpr uint32_t kAssumedDecodedFrameUs = 70000;

  struct SentRecord {
    TxJob job;
    uint32_t startMs = 0;
    size_t framesPlanned = 0;
    size_t framesSent = 0;
    bool completed = false;
    bool cancelled = false;
    bool faulted = false;
  };

  explicit FakeIrTransmitter(const IClock& clock) : clock_(clock) {}

  Status submit(const TxJob& job) override {
    if (state_ == TxState::Sending || state_ == TxState::Cancelling) {
      return Status::fail(Err::Unavailable, "transmitter busy");
    }
    if (failNextSubmit_) {
      failNextSubmit_ = false;
      state_ = TxState::Failed;
      return Status::fail(Err::Unavailable, "transmitter hardware fault (injected)");
    }
    frames_.clear();
    uint32_t cursor = 0;
    auto addFrame = [&](const Frame& f) {
      uint32_t len = kAssumedDecodedFrameUs;
      if (f.kind == Frame::Kind::Raw) {
        len = 0;
        for (uint16_t d : f.raw.durationsUs) len += d;
      }
      cursor += len;
      frames_.push_back(cursor);
      cursor += job.signal.repeatGapUs;
    };
    for (const Frame& f : job.signal.initial) addFrame(f);
    for (uint8_t i = 0; i < job.signal.repeatCount; ++i) {
      for (const Frame& f : job.signal.repeat) addFrame(f);
    }
    startUs_ = clock_.nowUs();
    cancelAtUs_ = 0;
    record_ = SentRecord{job, clock_.nowMs(), frames_.size(), 0, false, false, false};
    state_ = TxState::Sending;
    return Status::success();
  }

  void service() override {
    if (state_ == TxState::Sending && faultMidSend_) {
      faultMidSend_ = false;
      record_.faulted = true;
      record_.framesSent = framesDone(clock_.nowUs() - startUs_);
      history_.push_back(record_);
      state_ = TxState::Failed;
      return;
    }
    if (state_ != TxState::Sending && state_ != TxState::Cancelling) return;
    const uint32_t elapsed = clock_.nowUs() - startUs_;
    if (state_ == TxState::Cancelling) {
      if (elapsed >= cancelAtUs_) {
        record_.cancelled = true;
        record_.framesSent = framesDone(cancelAtUs_);
        history_.push_back(record_);
        state_ = TxState::Idle;
      }
      return;
    }
    if (!frames_.empty() && elapsed >= frames_.back()) {
      record_.completed = true;
      record_.framesSent = frames_.size();
      history_.push_back(record_);
      state_ = TxState::Idle;
    }
  }

  void cancel() override {
    if (state_ != TxState::Sending) return;
    const uint32_t elapsed = clock_.nowUs() - startUs_;
    // A frame already on the air is finished: cancel lands at the end of the current frame.
    cancelAtUs_ = frames_.back();
    for (uint32_t end : frames_) {
      if (end > elapsed) { cancelAtUs_ = end; break; }
    }
    state_ = TxState::Cancelling;
  }

  TxState state() const override { return state_; }

  // Test control / inspection
  void failNextSubmit() { failNextSubmit_ = true; }
  void faultDuringNextService() { faultMidSend_ = true; }
  const std::vector<SentRecord>& history() const { return history_; }

 private:
  size_t framesDone(uint32_t atUs) const {
    size_t n = 0;
    for (uint32_t end : frames_) if (end <= atUs) ++n;
    return n;
  }

  const IClock& clock_;
  TxState state_ = TxState::Idle;
  std::vector<uint32_t> frames_;  // end time of each frame, relative to start
  uint32_t startUs_ = 0, cancelAtUs_ = 0;
  SentRecord record_;
  std::vector<SentRecord> history_;
  bool failNextSubmit_ = false, faultMidSend_ = false;
};

}  // namespace iris::testkit
