// Sends a sequence of signals one at a time with a pause between them: the engine under the loop, crazy and Be Gone modes.
// It sends nothing by itself (start() is called by a screen after the user chose to), every send goes through
// TransmitService (so the emitter limits apply), it can be stopped at any time and it never blocks.
// The emitter gives no feedback: this class cannot tell whether a device reacted.
#pragma once

#include <memory>
#include <string>

#include "app/TransmitService.h"
#include "domain/IrSignal.h"
#include "hardware/IClock.h"
#include "hardware/ILogSink.h"

namespace iris {

struct SequenceItem {
  std::string label;  // what the screen shows as "last sent" (a button name, "Remote / Button"...)
  IrSignal signal;
};

// Where the signals come from. next() may do slow work (read a file from the card): it is called once per send, never
// while the transmitter is busy. Returns false when the sequence is over.
class ISignalSequence {
 public:
  virtual ~ISignalSequence() = default;
  virtual bool next(SequenceItem& out) = 0;
  // A short description of progress for the screen ("12/80", "LG / TV"); empty when there is nothing to say.
  virtual std::string progress() const { return {}; }
  // The pause to leave after the item just sent; `defaultGapMs` is what the screen asked for. A mode that wants a varying
  // pause (crazy mode) overrides this. The runner still never goes under SequenceRunner::kMinGapMs.
  virtual uint32_t gapAfterMs(uint32_t defaultGapMs) { return defaultGapMs; }
};

enum class SequenceState : uint8_t { Idle, Running, Done, Stopped };

class SequenceRunner {
 public:
  static constexpr uint32_t kMinGapMs = 100;  // however short the screen asks for, never faster than this

  SequenceRunner(TransmitService& tx, const IClock& clock, ILogSink* log = nullptr) : tx_(tx), clock_(clock), log_(log) {}

  // Starts a run (replaces a finished one; refused while running). `gapMs` is the pause after each send finishes, at least
  // kMinGapMs. `maxSends` 0 means until the sequence ends or stop().
  Status start(std::unique_ptr<ISignalSequence> sequence, uint32_t gapMs, uint32_t maxSends = 0);
  void stop();
  void reset();    // back to Idle; not while running
  void service();  // main loop

  SequenceState state() const { return state_; }
  bool running() const { return state_ == SequenceState::Running; }
  uint32_t sent() const { return sent_; }
  uint32_t skipped() const { return skipped_; }  // items the transmitter refused as invalid
  const std::string& last() const { return last_; }
  std::string progress() const { return sequence_ ? sequence_->progress() : std::string(); }
  uint32_t gapMs() const { return gapMs_; }

 private:
  TransmitService& tx_;
  const IClock& clock_;
  ILogSink* log_;
  std::unique_ptr<ISignalSequence> sequence_;
  SequenceState state_ = SequenceState::Idle;
  uint32_t gapMs_ = 1000, maxSends_ = 0, sent_ = 0, skipped_ = 0;
  uint32_t nextAtMs_ = 0;
  bool inFlight_ = false;
  std::string last_;
};

}  // namespace iris
