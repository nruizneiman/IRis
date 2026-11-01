#pragma once

#include <Arduino.h>
#include <IRrecv.h>

#include <atomic>

#include "domain/Limits.h"
#include "hardware/IClock.h"
#include "hardware/IIrReceiver.h"
#include "ir/IrCodec.h"

// One TSOP-style demodulating receiver on a GPIO, using IRremoteESP8266's timer-driven capture.
// Pure repeat codes and overflowed frames are handled by captureFromResult(); the carrier is never
// reported (the receiver only sees the demodulated envelope).
class IrReceiverAdapter : public iris::IIrReceiver {
 public:
  // Buffer holds the largest RAW frame the app accepts (+1: rawbuf[0] is the idle gap).
  static constexpr uint16_t kBufferEntries = static_cast<uint16_t>(iris::limits::kMaxRawEntries + 1);
  static constexpr uint32_t kQuietAfterTxMs = 40;  // ignore our own emission and its echo

  IrReceiverAdapter(uint8_t pin, uint8_t index, const iris::IClock& clock)
      : index_(index), clock_(clock), irrecv_(pin, kBufferEntries, kTimeoutMs, /*save_buffer=*/true) {}

  iris::Status start() override {
    if (!running_) {
      irrecv_.enableIRIn(/*pullup=*/false);  // the TSOP output has its own pull-up
      running_ = true;
    }
    return iris::Status::success();
  }

  void stop() override {
    if (running_) irrecv_.disableIRIn();
    running_ = false;
  }

  bool running() const override { return running_; }

  std::optional<iris::ReceivedCapture> poll() override {
    if (!running_) return std::nullopt;
    decode_results results;
    if (!irrecv_.decode(&results)) return std::nullopt;  // save_buffer=true: the library re-arms itself
    ++seen_;
    const uint32_t now = clock_.nowMs();
    if (suspended_ || static_cast<int32_t>(now - quietUntilMs_) < 0) {
      ++droppedOwnTx_;
      return std::nullopt;
    }
    auto capture = iris::captureFromResult(results, index_, now);
    if (!capture) ++droppedRepeat_;
    return capture;
  }

  // Called by the transmitter task while it emits, so the receiver does not learn our own signal.
  void suspend(bool on) {
    if (!on) quietUntilMs_ = clock_.nowMs() + kQuietAfterTxMs;
    suspended_ = on;
  }

  uint32_t seen() const { return seen_; }
  uint32_t droppedRepeat() const { return droppedRepeat_; }
  uint32_t droppedOwnTx() const { return droppedOwnTx_; }

 private:
  uint8_t index_;
  const iris::IClock& clock_;
  IRrecv irrecv_;
  bool running_ = false;
  std::atomic<bool> suspended_{false};
  std::atomic<uint32_t> quietUntilMs_{0};
  uint32_t seen_ = 0, droppedRepeat_ = 0, droppedOwnTx_ = 0;
};
