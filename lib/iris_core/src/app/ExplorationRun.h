// Executes an ExplorationPlan one code at a time. Bounded by the plan's hard caps, needs explicit
// confirmation above the threshold, can be stopped at any time, never starts by itself.
// The emitter gives no feedback: this class cannot tell whether the target device reacted.
#pragma once

#include <string>
#include <vector>

#include "app/ExplorationPlan.h"
#include "app/TransmitService.h"
#include "hardware/IBlobStore.h"
#include "hardware/IClock.h"
#include "hardware/ILogSink.h"

namespace iris {

enum class ExplorationState : uint8_t {
  Idle,
  AwaitingConfirmation,  // plan is long; start() is refused until confirm()
  Ready,
  Running,
  Stopped,
  Completed,
  Failed,
};

struct ExplorationLogEntry {
  uint32_t index = 0;
  uint32_t address = 0;
  uint32_t command = 0;
  uint32_t timeMs = 0;
};

class ExplorationRun {
 public:
  ExplorationRun(TransmitService& tx, const IClock& clock, ILogSink* log = nullptr)
      : tx_(tx), clock_(clock), log_(log) {}

  Status prepare(const ExplorationPlan& plan);  // from Idle or any finished state
  Status confirm();                             // AwaitingConfirmation -> Ready
  Status start();                               // Ready -> Running
  void stop();                                  // Running -> Stopped, cancels the transmitter
  void reset();                                 // back to Idle (not while Running)
  void tick();                                  // call from the main loop

  ExplorationState state() const { return state_; }
  const Status& lastStatus() const { return status_; }
  uint32_t sentCount() const { return static_cast<uint32_t>(log_entries_.size()); }
  uint32_t totalCount() const { return total_; }
  const std::vector<ExplorationLogEntry>& entries() const { return log_entries_; }

  std::string logText() const;
  // Persists the log of this run (overwrites the key). Size is bounded by the plan hard cap.
  Status saveLog(IBlobStore& store, const std::string& key) const;

 private:
  void fail(Status st);

  TransmitService& tx_;
  const IClock& clock_;
  ILogSink* log_;
  ExplorationState state_ = ExplorationState::Idle;
  Status status_;
  ExplorationPlan plan_;
  uint32_t total_ = 0, next_ = 0;
  bool waitingForTx_ = false;
  bool sentAny_ = false;
  uint32_t lastEndMs_ = 0;
  std::vector<ExplorationLogEntry> log_entries_;
};

}  // namespace iris
