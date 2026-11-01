// Cross-checks the domain <-> IRremoteESP8266 mapping on the PC, using the library's own unit-test
// helpers (IRsendTest records the waveform, IRrecv decodes it). What this proves: our field widths and
// encodings agree with what the library itself sends and decodes. What it does NOT prove: anything
// about real receivers, emitters, carriers, optics or timing tolerance on the device.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "FakeClock.h"
#include "FakeIrReceiver.h"
#include "IRsend_test.h"
#include "app/LearnSession.h"
#include "ir/IrCodec.h"

using namespace iris;

namespace {

constexpr uint16_t kPin = 4;

DecodedFrame frame(Protocol p, uint32_t addr, uint32_t cmd, uint16_t bits, bool toggle = false) {
  DecodedFrame d;
  d.protocol = p;
  d.address = addr;
  d.command = cmd;
  d.bits = bits;
  d.toggle = toggle;
  return d;
}

// Encode with the library, "transmit" into IRsendTest, then decode with IRrecv.
std::optional<DecodedFrame> roundTrip(const DecodedFrame& f) {
  IRsendTest tx(kPin);
  tx.begin();
  auto lf = toLibraryFrame(tx, f);
  if (!lf) return std::nullopt;
  tx.send(lf->type, lf->data, lf->nbits, 0);  // exactly one copy
  tx.makeDecodeResult();
  IRrecv rx(kPin);
  if (!rx.decode(&tx.capture)) return std::nullopt;
  return fromLibraryResult(tx.capture);
}

void expectRoundTrip(const DecodedFrame& f) {
  INFO("protocol=" << int(f.protocol) << " bits=" << f.bits << " addr=" << f.address << " cmd=" << f.command
                   << " toggle=" << f.toggle);
  auto back = roundTrip(f);
  REQUIRE(back.has_value());
  CHECK(back->protocol == f.protocol);
  // Finding from this cross-check: an RC5X (13-bit) word whose command is < 64 has the same wire
  // format as plain RC5 (the second start bit is the inverted 7th command bit), so any receiver,
  // including the library, reports it as the 12-bit variant.
  const uint16_t expectedBits = (f.protocol == Protocol::Rc5 && f.bits == 13 && f.command < 64) ? 12 : f.bits;
  CHECK(back->bits == expectedBits);
  // Second finding: an "extended" NEC address whose high byte is the bitwise inverse of its low byte
  // (e.g. 0xF20D) is on the wire identical to a standard 8-bit address (0x0D) and decodes as such.
  const bool necAmbiguous = f.protocol == Protocol::Nec && f.address > 0xFF &&
                            (((f.address >> 8) & 0xFF) == ((~f.address) & 0xFF));
  CHECK(back->address == (necAmbiguous ? (f.address & 0xFF) : f.address));
  CHECK(back->command == f.command);
  CHECK(back->toggle == f.toggle);
}

uint32_t lcg(uint32_t& s) {
  s = s * 1664525u + 1013904223u;
  return s >> 8;
}

}  // namespace

TEST_CASE("every catalogue variant round-trips at its limits") {
  for (size_t i = 0; i < protocolCount(); ++i) {
    const ProtocolInfo* p = protocolAt(i);
    for (uint8_t v = 0; v < p->variantCount; ++v) {
      const ProtocolVariant& var = p->variants[v];
      const uint32_t maxA = maskForBits(var.addressBits), maxC = maskForBits(var.commandBits);
      INFO(p->key << " " << var.bits << " bits");
      expectRoundTrip(frame(p->id, 0, 0, var.bits));
      expectRoundTrip(frame(p->id, maxA, maxC, var.bits));
      expectRoundTrip(frame(p->id, maxA, 0, var.bits));
      expectRoundTrip(frame(p->id, 0, maxC, var.bits));
    }
  }
}

TEST_CASE("random values inside the catalogue round-trip") {
  uint32_t seed = 12345;
  for (size_t i = 0; i < protocolCount(); ++i) {
    const ProtocolInfo* p = protocolAt(i);
    for (uint8_t v = 0; v < p->variantCount; ++v) {
      const ProtocolVariant& var = p->variants[v];
      for (int n = 0; n < 150; ++n) {
        uint32_t a = lcg(seed) & maskForBits(var.addressBits);
        const uint32_t c = lcg(seed) & maskForBits(var.commandBits);
        // NEC addresses 0x00..0xFF are sent as standard NEC (8-bit + inverse); 16-bit ones as extended NEC.
        // Both decode back to the same number, except that no extended address can be <= 0xFF.
        expectRoundTrip(frame(p->id, a, c, var.bits, p->hasToggle && (n & 1)));
      }
    }
  }
}

TEST_CASE("RC5 toggle bit survives the round trip in both bit-lengths") {
  for (bool t : {false, true}) {
    expectRoundTrip(frame(Protocol::Rc5, 5, 12, 12, t));
    expectRoundTrip(frame(Protocol::Rc5, 31, 127, 13, t));
  }
}

TEST_CASE("Sony 20-bit commands use the extended bits as the upper command bits") {
  expectRoundTrip(frame(Protocol::Sony, 1, 0x7F, 20));
  expectRoundTrip(frame(Protocol::Sony, 1, 0x80, 20));
  expectRoundTrip(frame(Protocol::Sony, 1, 0x7FFF, 20));
  IRsendTest tx(kPin);
  CHECK_FALSE(toLibraryFrame(tx, frame(Protocol::Sony, 1, 0x8000, 20)).has_value());
}

TEST_CASE("invalid frames never reach the library encoders") {
  IRsendTest tx(kPin);
  CHECK_FALSE(toLibraryFrame(tx, frame(Protocol::Unknown, 0, 0, 32)).has_value());
  CHECK_FALSE(toLibraryFrame(tx, frame(Protocol::Nec, 0, 0x100, 32)).has_value());
  CHECK_FALSE(toLibraryFrame(tx, frame(Protocol::Nec, 0x10000, 0, 32)).has_value());
  CHECK_FALSE(toLibraryFrame(tx, frame(Protocol::Nec, 0, 0, 16)).has_value());
  CHECK_FALSE(toLibraryFrame(tx, frame(Protocol::Rc5, 0, 0x40, 12)).has_value());
}

TEST_CASE("protocols that need repeats are marked for the transmitter") {
  IRsendTest tx(kPin);
  CHECK(toLibraryFrame(tx, frame(Protocol::Sony, 1, 1, 12))->repeat >= 2);  // Sony devices expect >= 3 copies
  CHECK(toLibraryFrame(tx, frame(Protocol::Nec, 1, 1, 32))->repeat == 0);
}

TEST_CASE("raw durations are converted from capture ticks to microseconds") {
  IRsendTest tx(kPin);
  tx.begin();
  tx.send(NEC, tx.encodeNEC(0x04, 0x08), 32, 0);
  tx.makeDecodeResult();
  const std::vector<uint16_t> raw = rawDurationsUs(tx.capture);
  REQUIRE(raw.size() >= 67);       // header pair + 32 bit pairs + stop mark (+ trailing gap)
  CHECK(raw[0] == 8960);           // nominal NEC leader mark in the library
  CHECK(raw[1] == 4480);
  CHECK(raw[2] == 560);
}

TEST_CASE("raw playback path: sendRaw output matches the durations that were given") {
  IRsendTest tx(kPin);
  tx.begin();
  const std::vector<uint16_t> in = {9000, 4500, 560, 560, 560, 1690, 560};
  tx.sendRaw(in.data(), static_cast<uint16_t>(in.size()), 36000);
  tx.makeDecodeResult();
  CHECK(rawDurationsUs(tx.capture) == in);
}

TEST_CASE("held-key repeat codes and overflows are not learned as new signals") {
  IRsendTest tx(kPin);
  tx.begin();
  tx.send(NEC, tx.encodeNEC(0x04, 0x08), 32, 1);  // frame + NEC repeat code
  tx.makeDecodeResult();
  IRrecv rx(kPin);
  REQUIRE(rx.decode(&tx.capture));
  CHECK(captureFromResult(tx.capture, 0, 0).has_value());  // first frame is a real capture

  decode_results rep = tx.capture;
  rep.repeat = true;
  CHECK_FALSE(captureFromResult(rep, 0, 0).has_value());

  decode_results over = tx.capture;
  over.overflow = true;
  auto c = captureFromResult(over, 1, 77);
  REQUIRE(c.has_value());
  CHECK(c->bufferOverflow);
  CHECK_FALSE(c->decoded.has_value());  // an overflowed capture is never trusted as decoded
  CHECK(c->receiverIndex == 1);
  CHECK(c->timestampMs == 77);
}

TEST_CASE("learn then replay: the stored decoded frame regenerates the same waveform") {
  // Library-generated NEC waveform stands in for what a receiver would hand over.
  IRsendTest original(kPin);
  original.begin();
  original.send(NEC, original.encodeNEC(0x12, 0x34), 32, 0);
  original.makeDecodeResult();
  IRrecv rx(kPin);
  REQUIRE(rx.decode(&original.capture));
  auto capture = captureFromResult(original.capture, 0, 0);
  REQUIRE(capture.has_value());
  REQUIRE(capture->decoded.has_value());

  testkit::FakeClock clock;
  testkit::FakeIrReceiver receiver;
  LearnSession session(receiver, clock);
  REQUIRE(session.start(1000).ok());
  receiver.push(*capture);
  session.tick();
  REQUIRE(session.state() == LearnState::Captured);
  const IrSignal& sig = *session.result();
  CHECK(sig.recognition == Recognition::Recognized);
  REQUIRE(sig.originalCapture.has_value());  // RAW kept next to the decoded frame

  IRsendTest replay(kPin);
  replay.begin();
  auto lf = toLibraryFrame(replay, sig.initial[0].decoded);
  REQUIRE(lf.has_value());
  replay.send(lf->type, lf->data, lf->nbits, 0);
  replay.makeDecodeResult();
  CHECK(rawDurationsUs(replay.capture) == rawDurationsUs(original.capture));
}
