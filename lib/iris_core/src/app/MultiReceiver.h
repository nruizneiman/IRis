// Presents several physical receivers (e.g. two TSOP38238 pointing in different directions) as one
// IIrReceiver. Captures are tagged with the index of the receiver that produced them; polling is
// round-robin so one noisy receiver cannot starve the other.
#pragma once

#include <vector>

#include "hardware/IIrReceiver.h"

namespace iris {

class MultiReceiver : public IIrReceiver {
 public:
  static constexpr size_t kMaxReceivers = 4;

  // Returns false (and ignores the receiver) beyond kMaxReceivers.
  bool add(IIrReceiver& receiver);
  size_t size() const { return receivers_.size(); }

  // Ok when at least one receiver started; the failed ones are reported by failedCount().
  Status start() override;
  void stop() override;
  bool running() const override;
  std::optional<ReceivedCapture> poll() override;

  size_t failedCount() const { return failed_; }

 private:
  std::vector<IIrReceiver*> receivers_;
  size_t next_ = 0;
  size_t failed_ = 0;
};

}  // namespace iris
