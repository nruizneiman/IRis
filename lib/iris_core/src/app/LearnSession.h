// Capture workflow: arm the receiver, wait (with timeout), analyse, build a candidate IrSignal.
// The session never stores anything: the caller decides (confirm/retry/cancel) and saves.
#pragma once

#include <optional>

#include "Result.h"
#include "domain/IrSignal.h"
#include "hardware/IClock.h"
#include "hardware/IIrReceiver.h"
#include "hardware/ILogSink.h"

namespace iris {

enum class LearnState : uint8_t {
  Idle,
  Armed,      // receiver running, waiting for a burst
  Captured,   // result() holds a candidate signal
  Rejected,   // capture was empty/too short/invalid: nothing to store
  TimedOut,   // nothing arrived in time
  Failed,     // receiver could not start
};

class LearnSession {
 public:
  static constexpr uint32_t kMaxTimeoutMs = 60000;

  LearnSession(IIrReceiver& receiver, const IClock& clock, ILogSink* log = nullptr)
      : receiver_(receiver), clock_(clock), log_(log) {}

  // Allowed from any state except Armed (cancel first).
  Status start(uint32_t timeoutMs);
  void cancel();  // back to Idle, receiver stopped
  void tick();    // call from the main loop

  LearnState state() const { return state_; }
  const Status& lastStatus() const { return status_; }
  const std::optional<IrSignal>& result() const { return result_; }
  // Hands the candidate to the caller and returns to Idle.
  std::optional<IrSignal> take();

 private:
  void finish(LearnState next, Status status);
  void handleCapture(ReceivedCapture capture);
  void note(LogLevel level, const std::string& msg) const;

  IIrReceiver& receiver_;
  const IClock& clock_;
  ILogSink* log_;
  LearnState state_ = LearnState::Idle;
  Status status_;
  std::optional<IrSignal> result_;
  uint32_t startMs_ = 0, timeoutMs_ = 0;
};

}  // namespace iris
