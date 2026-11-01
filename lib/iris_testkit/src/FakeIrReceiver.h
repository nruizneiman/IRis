#pragma once

#include <deque>

#include "hardware/IIrReceiver.h"

namespace iris::testkit {

// Feeds scripted captures. The "decoded" part of a capture is whatever the test supplies: this
// fake does not decode, so it says nothing about what the real library would recognise.
class FakeIrReceiver : public IIrReceiver {
 public:
  Status start() override {
    if (failStart_) return Status::fail(Err::Unavailable, "receiver start failed (injected)");
    running_ = true;
    return Status::success();
  }
  void stop() override { running_ = false; }
  bool running() const override { return running_; }

  std::optional<ReceivedCapture> poll() override {
    if (!running_ || queue_.empty()) return std::nullopt;
    ReceivedCapture c = std::move(queue_.front());
    queue_.pop_front();
    return c;
  }

  // Captures pushed while the receiver is stopped are kept and delivered after start().
  void push(ReceivedCapture c) { queue_.push_back(std::move(c)); }
  void setFailStart(bool f) { failStart_ = f; }
  size_t pending() const { return queue_.size(); }

 private:
  std::deque<ReceivedCapture> queue_;
  bool running_ = false;
  bool failStart_ = false;
};

}  // namespace iris::testkit
