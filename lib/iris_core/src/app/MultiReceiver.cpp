#include "app/MultiReceiver.h"

namespace iris {

bool MultiReceiver::add(IIrReceiver& receiver) {
  if (receivers_.size() >= kMaxReceivers) return false;
  receivers_.push_back(&receiver);
  return true;
}

Status MultiReceiver::start() {
  if (receivers_.empty()) return Status::fail(Err::Unavailable, "no receiver configured");
  failed_ = 0;
  Status firstError;
  size_t started = 0;
  for (IIrReceiver* r : receivers_) {
    Status st = r->start();
    if (st.ok()) {
      ++started;
    } else {
      ++failed_;
      if (firstError.ok()) firstError = st;
    }
  }
  return started > 0 ? Status::success() : firstError;
}

void MultiReceiver::stop() {
  for (IIrReceiver* r : receivers_) r->stop();
}

bool MultiReceiver::running() const {
  for (const IIrReceiver* r : receivers_) {
    if (r->running()) return true;
  }
  return false;
}

std::optional<ReceivedCapture> MultiReceiver::poll() {
  const size_t n = receivers_.size();
  for (size_t i = 0; i < n; ++i) {
    const size_t idx = (next_ + i) % n;
    if (auto c = receivers_[idx]->poll()) {
      c->receiverIndex = static_cast<uint8_t>(idx);
      next_ = (idx + 1) % n;
      return c;
    }
  }
  return std::nullopt;
}

}  // namespace iris
