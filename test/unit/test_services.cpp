#include <doctest.h>

#include "FakeClock.h"
#include "FakeIrReceiver.h"
#include "FakeIrTransmitter.h"
#include "Fixtures.h"
#include "MemoryBlobStore.h"
#include "app/ExplorationRun.h"
#include "app/LearnSession.h"
#include "app/SignalBuilder.h"
#include "app/TransmitService.h"
#include "diagnostics/RingLogSink.h"
#include "domain/Limits.h"

using namespace iris;
using namespace iris::testkit;

namespace {

ReceivedCapture necCapture() {
  ReceivedCapture c;
  c.raw.durationsUs = syntheticNecFrame(0xF708FF00u);
  DecodedFrame d;
  d.protocol = Protocol::Nec;
  d.address = 0;
  d.command = 8;
  d.bits = 32;
  c.decoded = d;
  return c;
}

struct LearnFixture {
  FakeClock clock;
  FakeIrReceiver rx;
  RingLogSink log{clock, 50};
  LearnSession session{rx, clock, &log};
};

}  // namespace

TEST_CASE("learn: recognised capture keeps RAW next to the decoded frame") {
  LearnFixture f;
  REQUIRE(f.session.start(5000).ok());
  CHECK(f.session.state() == LearnState::Armed);
  f.rx.push(necCapture());
  f.session.tick();
  REQUIRE(f.session.state() == LearnState::Captured);
  const IrSignal& s = *f.session.result();
  CHECK(s.recognition == Recognition::Recognized);
  CHECK(s.provenance == Provenance::Learned);
  CHECK(s.initial[0].kind == Frame::Kind::Decoded);
  REQUIRE(s.originalCapture.has_value());
  CHECK(s.originalCapture->carrierHz == 0);  // unknown after a demodulating receiver
  CHECK_FALSE(f.rx.running());
  auto taken = f.session.take();
  CHECK(taken.has_value());
  CHECK(f.session.state() == LearnState::Idle);
  CHECK_FALSE(f.session.take().has_value());
}

TEST_CASE("learn: unknown pattern is kept as RAW, not discarded") {
  LearnFixture f;
  f.session.start(5000);
  ReceivedCapture c;
  c.raw.durationsUs = syntheticUnknownPattern();
  f.rx.push(c);
  f.session.tick();
  REQUIRE(f.session.state() == LearnState::Captured);
  CHECK(f.session.result()->recognition == Recognition::Unknown);
  CHECK(f.session.result()->initial[0].kind == Frame::Kind::Raw);
}

TEST_CASE("learn: empty and too-short captures are rejected") {
  LearnFixture f;
  f.session.start(5000);
  f.rx.push(ReceivedCapture{});
  f.session.tick();
  CHECK(f.session.state() == LearnState::Rejected);
  CHECK_FALSE(f.session.result().has_value());
  CHECK(f.log.contains("rejected"));

  f.session.start(5000);
  ReceivedCapture shortCap;
  shortCap.raw.durationsUs = {500, 500, 500};
  f.rx.push(shortCap);
  f.session.tick();
  CHECK(f.session.state() == LearnState::Rejected);
}

TEST_CASE("learn: noisy or overflowed decoded captures are only Partial") {
  LearnFixture f;
  f.session.start(5000);
  ReceivedCapture noisy;
  noisy.raw.durationsUs = syntheticNoiseCapture();
  noisy.decoded = necCapture().decoded;
  f.rx.push(noisy);
  f.session.tick();
  REQUIRE(f.session.state() == LearnState::Captured);
  CHECK(f.session.result()->recognition == Recognition::Partial);
  CHECK(f.session.result()->quality.possibleNoise);

  f.session.start(5000);
  ReceivedCapture overflow = necCapture();
  overflow.bufferOverflow = true;
  f.rx.push(overflow);
  f.session.tick();
  REQUIRE(f.session.state() == LearnState::Captured);
  CHECK(f.session.result()->recognition == Recognition::Partial);
  CHECK(f.session.result()->quality.truncated);
}

TEST_CASE("learn: decoder values that fail validation fall back to RAW as Partial") {
  LearnFixture f;
  f.session.start(5000);
  ReceivedCapture c = necCapture();
  c.decoded->command = 0x1FF;  // impossible for NEC
  f.rx.push(c);
  f.session.tick();
  REQUIRE(f.session.state() == LearnState::Captured);
  CHECK(f.session.result()->recognition == Recognition::Partial);
  CHECK(f.session.result()->initial[0].kind == Frame::Kind::Raw);
}

TEST_CASE("learn: oversized capture is truncated and flagged") {
  LearnFixture f;
  f.session.start(5000);
  ReceivedCapture c;
  c.raw.durationsUs.assign(limits::kMaxRawEntries + 200, 500);
  f.rx.push(c);
  f.session.tick();
  REQUIRE(f.session.state() == LearnState::Captured);
  CHECK(f.session.result()->quality.truncated);
  CHECK(f.session.result()->originalCapture->durationsUs.size() == limits::kMaxRawEntries);
}

TEST_CASE("learn: timeout, cancel, receiver failure and bad arguments") {
  LearnFixture f;
  CHECK(f.session.start(0).code == Err::OutOfRange);
  CHECK(f.session.start(LearnSession::kMaxTimeoutMs + 1).code == Err::OutOfRange);

  REQUIRE(f.session.start(1000).ok());
  CHECK(f.session.start(1000).code == Err::Unavailable);  // already armed
  f.clock.advanceMs(999);
  f.session.tick();
  CHECK(f.session.state() == LearnState::Armed);
  f.clock.advanceMs(1);
  f.session.tick();
  CHECK(f.session.state() == LearnState::TimedOut);
  CHECK_FALSE(f.rx.running());

  REQUIRE(f.session.start(1000).ok());
  f.session.cancel();
  CHECK(f.session.state() == LearnState::Idle);
  CHECK_FALSE(f.rx.running());

  f.rx.setFailStart(true);
  CHECK(f.session.start(1000).code == Err::Unavailable);
  CHECK(f.session.state() == LearnState::Failed);
}

TEST_CASE("learn: timeout is wraparound safe") {
  LearnFixture f;
  f.clock.setUs((1ull << 32) * 1000ull - 500000ull);
  REQUIRE(f.session.start(1000).ok());
  f.clock.advanceMs(900);
  f.session.tick();
  CHECK(f.session.state() == LearnState::Armed);
  f.clock.advanceMs(200);  // crosses the wrap
  f.session.tick();
  CHECK(f.session.state() == LearnState::TimedOut);
}

TEST_CASE("transmit service validates, refuses when busy, never sends by itself") {
  FakeClock clock;
  FakeIrTransmitter tx(clock);
  RingLogSink log(clock, 20);
  TransmitService svc(tx, &log);
  CHECK(tx.history().empty());

  IrSignal invalid;
  CHECK_FALSE(svc.send(invalid).ok());
  CHECK(tx.state() == TxState::Idle);  // never reached the hardware
  CHECK(log.contains("refused"));

  IrSignal good = buildDecodedSignal(Protocol::Nec, 0, 8, 32).value();
  CHECK(svc.send(good, 0).code == Err::InvalidArgument);
  CHECK(svc.send(good, 1, 5).code == Err::OutOfRange);
  REQUIRE(svc.send(good).ok());
  CHECK(svc.send(good).code == Err::Unavailable);
  CHECK(svc.acceptedCount() == 1);
  clock.advanceMs(200);
  svc.service();
  CHECK_FALSE(svc.busy());
  CHECK(log.contains("finished"));

  tx.failNextSubmit();
  CHECK_FALSE(svc.send(good).ok());
  CHECK(svc.acceptedCount() == 1);
  svc.service();
  CHECK(log.contains("fault"));
}

namespace {
struct ExploreFixture {
  FakeClock clock;
  FakeIrTransmitter tx{clock};
  RingLogSink log{clock, 100};
  TransmitService svc{tx, &log};
  ExplorationRun run{svc, clock, &log};

  void runFor(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += 10) {
      clock.advanceMs(10);
      svc.service();
      run.tick();
    }
  }
  static ExplorationPlan plan(uint32_t cmdLast, uint32_t maxTx, uint32_t gap = 300) {
    ExplorationPlan p;
    p.protocol = Protocol::Nec;
    p.bits = 32;
    p.addressFirst = p.addressLast = 0;
    p.commandFirst = 0;
    p.commandLast = cmdLast;
    p.maxTransmissions = maxTx;
    p.gapMs = gap;
    return p;
  }
};
}  // namespace

TEST_CASE("exploration run: small plan runs to completion honouring the gap") {
  ExploreFixture f;
  REQUIRE(f.run.prepare(ExploreFixture::plan(2, 100)).ok());  // 3 codes
  CHECK(f.run.state() == ExplorationState::Ready);
  f.runFor(500);
  CHECK(f.tx.history().empty());  // nothing is sent before start()
  REQUIRE(f.run.start().ok());
  f.runFor(5000);
  CHECK(f.run.state() == ExplorationState::Completed);
  REQUIRE(f.run.entries().size() == 3);
  CHECK(f.tx.history().size() == 3);
  for (size_t i = 1; i < f.run.entries().size(); ++i) {
    const uint32_t spacing = f.run.entries()[i].timeMs - f.run.entries()[i - 1].timeMs;
    CHECK(spacing >= 70 + 300);  // frame time + configured gap
  }
  CHECK(f.run.logText().find("s 0/2\n") != std::string::npos);  // "#2 <time>s <address>/<command>"
}

TEST_CASE("exploration run: long plans need explicit confirmation") {
  ExploreFixture f;
  REQUIRE(f.run.prepare(ExploreFixture::plan(255, 100)).ok());  // 100 codes > threshold
  CHECK(f.run.state() == ExplorationState::AwaitingConfirmation);
  CHECK(f.run.start().code == Err::InvalidArgument);
  f.runFor(1000);
  CHECK(f.tx.history().empty());
  REQUIRE(f.run.confirm().ok());
  CHECK(f.run.start().ok());
  CHECK(f.run.confirm().code == Err::InvalidArgument);
}

TEST_CASE("exploration run: stop is immediate for the run and cancels the transmitter") {
  ExploreFixture f;
  REQUIRE(f.run.prepare(ExploreFixture::plan(10, 100)).ok());
  REQUIRE(f.run.start().ok());
  f.runFor(30);  // first code is on the air (assumed 70 ms frame)
  REQUIRE(f.run.sentCount() == 1);
  CHECK(f.tx.busy());
  f.run.stop();
  CHECK(f.run.state() == ExplorationState::Stopped);
  const uint32_t sentAtStop = f.run.sentCount();
  f.runFor(5000);
  CHECK(f.run.sentCount() == sentAtStop);  // nothing more is sent
  CHECK_FALSE(f.tx.busy());
  CHECK(f.tx.history().back().cancelled);
}

TEST_CASE("exploration run: invalid plans, double start and hardware faults") {
  ExploreFixture f;
  ExplorationPlan bad = ExploreFixture::plan(10, limits::kExplorationHardMaxTransmissions + 1);
  CHECK(f.run.prepare(bad).code == Err::LimitReached);
  CHECK(f.run.state() == ExplorationState::Idle);
  CHECK(f.run.start().code == Err::InvalidArgument);

  REQUIRE(f.run.prepare(ExploreFixture::plan(10, 100)).ok());
  REQUIRE(f.run.start().ok());
  CHECK(f.run.prepare(ExploreFixture::plan(2, 5)).code == Err::Unavailable);  // can't re-plan while running
  f.tx.faultDuringNextService();
  f.runFor(300);
  CHECK(f.run.state() == ExplorationState::Failed);
  CHECK(f.log.contains("failed"));

  f.run.reset();
  f.tx.failNextSubmit();
  REQUIRE(f.run.prepare(ExploreFixture::plan(10, 100)).ok());
  REQUIRE(f.run.start().ok());
  f.runFor(50);
  CHECK(f.run.state() == ExplorationState::Failed);
}

TEST_CASE("exploration run: start is refused while the transmitter is busy") {
  ExploreFixture f;
  REQUIRE(f.svc.send(buildDecodedSignal(Protocol::Nec, 0, 1, 32).value()).ok());
  REQUIRE(f.run.prepare(ExploreFixture::plan(2, 5)).ok());
  CHECK(f.run.start().code == Err::Unavailable);
}

TEST_CASE("exploration run: log can be persisted") {
  ExploreFixture f;
  MemoryBlobStore store;
  REQUIRE(f.run.prepare(ExploreFixture::plan(1, 5)).ok());
  REQUIRE(f.run.start().ok());
  f.runFor(2000);
  REQUIRE(f.run.saveLog(store, "explore.log").ok());
  CHECK(store.read("explore.log").value().find("#1") != std::string::npos);
  store.failWritesAfter(0);
  CHECK_FALSE(f.run.saveLog(store, "explore.log").ok());
}
