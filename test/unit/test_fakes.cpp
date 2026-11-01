#include <doctest.h>

#include "FakeBattery.h"
#include "FakeButtons.h"
#include "FakeClock.h"
#include "FakeDisplay.h"
#include "FakeIrReceiver.h"
#include "FakeIrTransmitter.h"
#include "Fixtures.h"
#include "app/SignalBuilder.h"
#include "diagnostics/RingLogSink.h"

using namespace iris;
using namespace iris::testkit;

TEST_CASE("fake display shows only flushed content and can fail") {
  FakeDisplay d;
  CHECK(d.begin().ok());
  d.drawText(0, 0, "Hola Ñandú", FontSize::Normal);
  CHECK_FALSE(d.showsText("Hola"));  // not flushed yet
  CHECK(d.flush().ok());
  CHECK(d.showsText("Ñandú"));
  d.clear();
  CHECK(d.showsText("Hola"));  // still the old frame until the next flush
  d.drawText(0, 0, "Otro", FontSize::Small);
  d.setFailFlush(true);
  CHECK(d.flush().code == Err::StorageFailure);
  CHECK(d.showsText("Hola"));
  FakeDisplay missing;
  missing.setFailBegin(true);
  CHECK(missing.begin().code == Err::Unavailable);
}

TEST_CASE("fake buttons and battery") {
  FakeButtons b;
  CHECK_FALSE(b.isPressed(Key::Ok));
  b.set(Key::Ok, true);
  CHECK(b.isPressed(Key::Ok));
  CHECK_FALSE(b.isPressed(Key::Up));
  FakeBattery bat;
  CHECK_FALSE(bat.readMillivolts().has_value());  // no circuit -> no invented value
  bat.set(3700);
  CHECK(*bat.readMillivolts() == 3700);
}

TEST_CASE("fake receiver delivers only while running") {
  FakeIrReceiver rx;
  ReceivedCapture c;
  c.raw.durationsUs = {500, 500};
  rx.push(c);
  CHECK_FALSE(rx.poll().has_value());
  REQUIRE(rx.start().ok());
  CHECK(rx.poll().has_value());
  CHECK_FALSE(rx.poll().has_value());
  rx.setFailStart(true);
  CHECK(rx.start().code == Err::Unavailable);
}

TEST_CASE("fake transmitter models time, busy state and gaps") {
  FakeClock clock;
  FakeIrTransmitter tx(clock);
  IrSignal s = syntheticNecSignalWithRepeats();  // 1 initial + 3 repeats of 11.25 ms + 40 ms gaps
  TxJob job;
  job.signal = s;
  REQUIRE(tx.submit(job).ok());
  CHECK(tx.busy());
  CHECK(tx.submit(job).code == Err::Unavailable);
  tx.service();
  CHECK(tx.busy());
  clock.advanceMs(1000);
  tx.service();
  CHECK_FALSE(tx.busy());
  REQUIRE(tx.history().size() == 1);
  CHECK(tx.history()[0].completed);
  CHECK(tx.history()[0].framesSent == 4);
}

TEST_CASE("fake transmitter cancel lands at the end of the current frame") {
  FakeClock clock;
  FakeIrTransmitter tx(clock);
  TxJob job;
  job.signal = syntheticNecSignalWithRepeats();
  REQUIRE(tx.submit(job).ok());
  clock.advanceMs(10);  // inside the first (70 ms assumed) frame
  tx.cancel();
  CHECK(tx.state() == TxState::Cancelling);
  clock.advanceMs(30);
  tx.service();
  CHECK(tx.state() == TxState::Cancelling);  // frame still on the air
  clock.advanceMs(40);
  tx.service();
  CHECK(tx.state() == TxState::Idle);
  REQUIRE(tx.history().size() == 1);
  CHECK(tx.history()[0].cancelled);
  CHECK(tx.history()[0].framesSent == 1);
  CHECK_FALSE(tx.history()[0].completed);
}

TEST_CASE("fake transmitter fault injection and retry") {
  FakeClock clock;
  FakeIrTransmitter tx(clock);
  TxJob job;
  job.signal = buildRawSignal({500, 500}).value();
  tx.failNextSubmit();
  CHECK(tx.submit(job).code == Err::Unavailable);
  CHECK(tx.state() == TxState::Failed);
  CHECK(tx.submit(job).ok());  // retry from Failed is allowed
  tx.faultDuringNextService();
  tx.service();
  CHECK(tx.state() == TxState::Failed);
  CHECK(tx.history().back().faulted);
}

TEST_CASE("ring log keeps only the newest entries") {
  FakeClock clock;
  RingLogSink log(clock, 3);
  for (int i = 0; i < 5; ++i) {
    clock.advanceMs(1);
    log.log(LogLevel::Info, "t", "m" + std::to_string(i));
  }
  CHECK(log.size() == 3);
  CHECK_FALSE(log.contains("m1"));
  CHECK(log.contains("m4"));
  CHECK(log.entries().front().timeMs == 3);
}
