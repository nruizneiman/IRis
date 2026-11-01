#include "app/ExplorationRun.h"

#include <cstdio>

#include "app/SignalBuilder.h"

namespace iris {

Status ExplorationRun::prepare(const ExplorationPlan& plan) {
  if (state_ == ExplorationState::Running) return Status::fail(Err::Unavailable, "run in progress");
  Status st = validateExplorationPlan(plan);
  if (!st.ok()) return st;
  plan_ = plan;
  total_ = explorationCandidateCount(plan);
  next_ = 0;
  sentAny_ = false;
  waitingForTx_ = false;
  log_entries_.clear();
  status_ = Status::success();
  state_ = explorationNeedsConfirmation(plan) ? ExplorationState::AwaitingConfirmation : ExplorationState::Ready;
  return Status::success();
}

Status ExplorationRun::confirm() {
  if (state_ != ExplorationState::AwaitingConfirmation) {
    return Status::fail(Err::InvalidArgument, "nothing to confirm");
  }
  state_ = ExplorationState::Ready;
  return Status::success();
}

Status ExplorationRun::start() {
  if (state_ == ExplorationState::AwaitingConfirmation) {
    return Status::fail(Err::InvalidArgument, "explicit confirmation required");
  }
  if (state_ != ExplorationState::Ready) return Status::fail(Err::InvalidArgument, "no prepared plan");
  if (tx_.busy()) return Status::fail(Err::Unavailable, "transmitter busy");
  state_ = ExplorationState::Running;
  if (log_) log_->log(LogLevel::Info, "explore", "started, " + std::to_string(total_) + " codes");
  return Status::success();
}

void ExplorationRun::stop() {
  if (state_ != ExplorationState::Running) return;
  tx_.cancel();
  state_ = ExplorationState::Stopped;
  if (log_) log_->log(LogLevel::Info, "explore", "stopped by user after " + std::to_string(sentCount()));
}

void ExplorationRun::reset() {
  if (state_ == ExplorationState::Running) return;
  state_ = ExplorationState::Idle;
  total_ = next_ = 0;
  log_entries_.clear();
  status_ = Status::success();
}

void ExplorationRun::fail(Status st) {
  status_ = std::move(st);
  state_ = ExplorationState::Failed;
  if (log_) log_->log(LogLevel::Error, "explore", "failed: " + status_.message);
}

void ExplorationRun::tick() {
  if (state_ != ExplorationState::Running) return;

  if (waitingForTx_) {
    if (tx_.state() == TxState::Failed) return fail(Status::fail(Err::Unavailable, "transmitter fault"));
    if (tx_.busy()) return;
    waitingForTx_ = false;
    lastEndMs_ = clock_.nowMs();
  }
  if (next_ >= total_) {
    state_ = ExplorationState::Completed;
    if (log_) log_->log(LogLevel::Info, "explore", "completed");
    return;
  }
  // Gap is measured from when the previous transmission was seen finished (tick resolution).
  if (sentAny_ && clock_.nowMs() - lastEndMs_ < plan_.gapMs) return;

  auto candidate = explorationCandidateAt(plan_, next_);
  if (!candidate.ok()) return fail(candidate.status());
  auto signal = buildDecodedSignal(plan_.protocol, candidate.value().address, candidate.value().command, plan_.bits);
  if (!signal.ok()) return fail(signal.status());
  Status st = tx_.send(signal.value());
  if (!st.ok()) return fail(st);

  log_entries_.push_back(ExplorationLogEntry{next_, candidate.value().address, candidate.value().command, clock_.nowMs()});
  ++next_;
  sentAny_ = true;
  waitingForTx_ = true;
}

std::string ExplorationRun::logText() const {
  std::string out;
  char line[80];
  for (const auto& e : log_entries_) {
    // "#<index> <seconds since start>s <address hex>/<command hex>": short enough for one OLED line.
    std::snprintf(line, sizeof line, "#%lu %lu.%lus %lX/%lX\n", static_cast<unsigned long>(e.index),
                  static_cast<unsigned long>(e.timeMs / 1000), static_cast<unsigned long>((e.timeMs % 1000) / 100),
                  static_cast<unsigned long>(e.address), static_cast<unsigned long>(e.command));
    out += line;
  }
  return out;
}

Status ExplorationRun::saveLog(IBlobStore& store, const std::string& key) const {
  return store.write(key, logText());
}

}  // namespace iris
