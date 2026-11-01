#pragma once

#include <cstdint>

#include "Result.h"
#include "domain/IrSignal.h"

namespace iris {

enum class TxState : uint8_t { Idle, Sending, Cancelling, Failed };

struct TxJob {
  IrSignal signal;
  uint8_t channelMask = 0x01;  // bit n = emitter channel n (room for several MOSFET channels)
  uint32_t defaultCarrierHz = 38000;  // used only for raw frames whose carrier is unknown
};

// Contract: submit() never blocks for the duration of a transmission. cancel() takes effect
// between frames (a frame already on the air is not cut). Implementations enforce their own
// safety limits (maximum mark length, duty cycle) regardless of what the job asks for.
// From Failed, submit() is allowed and acts as a retry (the adapter re-initialises its hardware).
class IIrTransmitter {
 public:
  virtual ~IIrTransmitter() = default;
  virtual Status submit(const TxJob& job) = 0;  // Err::Unavailable while busy
  virtual void service() = 0;                   // call from the main loop; advances state
  virtual void cancel() = 0;
  virtual TxState state() const = 0;
  bool busy() const { return state() == TxState::Sending || state() == TxState::Cancelling; }
};

}  // namespace iris
