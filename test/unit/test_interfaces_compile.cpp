// Compile-time check that every hardware interface header is self-contained and implementable.
#include <doctest.h>

#include "FakeClock.h"
#include "hardware/IBatteryMonitor.h"
#include "hardware/IButtonInput.h"
#include "hardware/IDisplay.h"
#include "hardware/IIrReceiver.h"
#include "hardware/IIrTransmitter.h"
#include "hardware/ILogSink.h"

using namespace iris;

namespace {
struct NullTx : IIrTransmitter {
  Status submit(const TxJob&) override { return Status::success(); }
  void service() override {}
  void cancel() override {}
  TxState state() const override { return state_; }
  TxState state_ = TxState::Idle;
};
}  // namespace

TEST_CASE("transmitter busy() derives from state") {
  NullTx tx;
  CHECK_FALSE(tx.busy());
  tx.state_ = TxState::Sending;
  CHECK(tx.busy());
  tx.state_ = TxState::Cancelling;
  CHECK(tx.busy());
  tx.state_ = TxState::Failed;
  CHECK_FALSE(tx.busy());
}

TEST_CASE("fake clock supports wraparound-safe elapsed time") {
  testkit::FakeClock clock;
  clock.setUs((1ull << 32) * 1000ull - 500000ull);  // 0.5 s before the 32-bit ms counter wraps
  const uint32_t start = clock.nowMs();
  clock.advanceMs(1000);
  CHECK(clock.nowMs() < start);          // wrapped
  CHECK(clock.nowMs() - start == 1000);  // unsigned subtraction still correct
}
