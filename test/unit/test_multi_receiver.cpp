#include <doctest.h>

#include "FakeClock.h"
#include "FakeIrReceiver.h"
#include "app/LearnSession.h"
#include "app/MultiReceiver.h"

using namespace iris;
using namespace iris::testkit;

namespace {
ReceivedCapture cap(uint16_t marker) {
  ReceivedCapture c;
  c.raw.durationsUs = {marker, 500};
  return c;
}
}  // namespace

TEST_CASE("multi receiver: no receivers is an error, limit is enforced") {
  MultiReceiver m;
  CHECK(m.start().code == Err::Unavailable);
  CHECK_FALSE(m.running());
  FakeIrReceiver r[MultiReceiver::kMaxReceivers + 1];
  for (size_t i = 0; i < MultiReceiver::kMaxReceivers; ++i) CHECK(m.add(r[i]));
  CHECK_FALSE(m.add(r[MultiReceiver::kMaxReceivers]));
  CHECK(m.size() == MultiReceiver::kMaxReceivers);
}

TEST_CASE("multi receiver: captures are tagged with the receiver index, round-robin") {
  FakeIrReceiver a, b;
  MultiReceiver m;
  m.add(a);
  m.add(b);
  REQUIRE(m.start().ok());
  CHECK(m.running());

  a.push(cap(1));
  a.push(cap(2));
  b.push(cap(3));
  auto c1 = m.poll();
  auto c2 = m.poll();
  auto c3 = m.poll();
  REQUIRE(c1);
  REQUIRE(c2);
  REQUIRE(c3);
  CHECK(c1->receiverIndex == 0);
  CHECK(c2->receiverIndex == 1);  // b is served before a's second capture
  CHECK(c3->receiverIndex == 0);
  CHECK_FALSE(m.poll().has_value());
}

TEST_CASE("multi receiver: one failing receiver does not stop the other") {
  FakeIrReceiver a, b;
  a.setFailStart(true);
  MultiReceiver m;
  m.add(a);
  m.add(b);
  CHECK(m.start().ok());
  CHECK(m.failedCount() == 1);
  b.push(cap(7));
  auto c = m.poll();
  REQUIRE(c);
  CHECK(c->receiverIndex == 1);

  b.setFailStart(true);
  m.stop();
  CHECK_FALSE(m.running());
  CHECK(m.start().code == Err::Unavailable);  // all failed
  CHECK(m.failedCount() == 2);
}

TEST_CASE("multi receiver works as the learn session's receiver") {
  FakeIrReceiver a, b;
  MultiReceiver m;
  m.add(a);
  m.add(b);
  FakeClock clock;
  LearnSession session(m, clock);
  REQUIRE(session.start(1000).ok());
  CHECK(a.running());
  CHECK(b.running());
  b.push(cap(100));
  ReceivedCapture ok;
  ok.raw.durationsUs = {9000, 4500, 560, 560, 560, 1690, 560};
  b.push(ok);
  session.tick();
  CHECK(session.state() == LearnState::Rejected);  // first capture is the too-short one
  CHECK_FALSE(a.running());
  CHECK_FALSE(b.running());
}
