#pragma once

#include <optional>

#include "Result.h"
#include "domain/IrSignal.h"

namespace iris {

// What the receiver adapter hands to the app. The decode attempt is made by the adapter (it owns
// the IR library); the raw timings are always included.
struct ReceivedCapture {
  RawFrame raw;                         // carrierHz is 0: a demodulating receiver hides it
  std::optional<DecodedFrame> decoded;  // set only when the library recognised a protocol
  bool bufferOverflow = false;
  uint8_t receiverIndex = 0;            // 0 = primary, 1 = secondary
  uint32_t timestampMs = 0;
};

class IIrReceiver {
 public:
  virtual ~IIrReceiver() = default;
  virtual Status start() = 0;
  virtual void stop() = 0;
  virtual bool running() const = 0;
  // Non-blocking. Returns a capture at most once per received burst.
  virtual std::optional<ReceivedCapture> poll() = 0;
};

}  // namespace iris
