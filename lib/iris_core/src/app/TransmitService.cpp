#include "app/TransmitService.h"

#include "domain/Limits.h"
#include "domain/SignalValidation.h"

namespace iris {

Status TransmitService::send(const IrSignal& signal, uint8_t channelMask, uint32_t defaultCarrierHz) {
  Status st = validateSignal(signal);
  if (!st.ok()) {
    if (log_) log_->log(LogLevel::Warn, "tx", "refused invalid signal: " + st.message);
    return st;
  }
  if (channelMask == 0) return Status::fail(Err::InvalidArgument, "no emitter channel selected");
  if (defaultCarrierHz < limits::kMinCarrierHz || defaultCarrierHz > limits::kMaxCarrierHz) {
    return Status::fail(Err::OutOfRange, "default carrier out of range");
  }
  TxJob job;
  job.signal = signal;
  job.channelMask = channelMask;
  job.defaultCarrierHz = defaultCarrierHz;
  st = tx_.submit(job);
  if (!st.ok()) {
    if (log_) log_->log(LogLevel::Error, "tx", "submit failed: " + st.message);
    return st;
  }
  ++accepted_;
  lastState_ = tx_.state();  // a short job may finish inside one service() call; still log its end
  if (log_) log_->log(LogLevel::Info, "tx", "started");
  return Status::success();
}

void TransmitService::service() {
  tx_.service();
  const TxState now = tx_.state();
  if (now != lastState_) {
    if (log_ && now == TxState::Failed) log_->log(LogLevel::Error, "tx", "transmitter fault");
    if (log_ && now == TxState::Idle && lastState_ != TxState::Idle) log_->log(LogLevel::Info, "tx", "finished");
    lastState_ = now;
  }
}

void TransmitService::cancel() {
  tx_.cancel();
  if (log_) log_->log(LogLevel::Info, "tx", "cancel requested");
}

}  // namespace iris
