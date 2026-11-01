// Single entry point for sending signals. Validates first; never blocks; never sends on its own.
#pragma once

#include "Result.h"
#include "domain/IrSignal.h"
#include "hardware/IIrTransmitter.h"
#include "hardware/ILogSink.h"

namespace iris {

class TransmitService {
 public:
  explicit TransmitService(IIrTransmitter& tx, ILogSink* log = nullptr) : tx_(tx), log_(log) {}

  // Err::Unavailable while busy. Invalid signals are refused before reaching the hardware.
  Status send(const IrSignal& signal, uint8_t channelMask = 0x01, uint32_t defaultCarrierHz = 38000);
  void service();  // call from the main loop
  void cancel();
  bool busy() const { return tx_.busy(); }
  TxState state() const { return tx_.state(); }
  uint32_t acceptedCount() const { return accepted_; }

 private:
  IIrTransmitter& tx_;
  ILogSink* log_;
  TxState lastState_ = TxState::Idle;
  uint32_t accepted_ = 0;
};

}  // namespace iris
