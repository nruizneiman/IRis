// Replays raw durations captured on the real bench (TSOP38238 receiving our own emitter, 2026-11-01, loopback `x`)
// through the PC build of IRremoteESP8266. The data are field measurements; the decoder is the library's own.
#include <doctest.h>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "IRrecv.h"
#include "IRsend_test.h"
#include "import/FlipperIr.h"
#include "ir/IrCodec.h"

namespace {

constexpr uint16_t kPin = 4;

// Same layout the library's test helper produces: rawbuf[0] unused, durations in capture ticks from index 1.
struct Replay {
  uint16_t buf[RAW_BUF] = {};
  decode_results res{};

  Replay(const std::vector<uint16_t>& us, bool trailingGap) {
    uint16_t n = 0;
    for (uint16_t d : us) buf[++n] = static_cast<uint16_t>(d / kRawTick);
    if (trailingGap) buf[++n] = static_cast<uint16_t>(100000 / kRawTick);
    res.rawbuf = buf;
    res.rawlen = static_cast<uint16_t>(n + 1);
    res.overflow = false;
    res.decode_type = UNKNOWN;
  }
};

// Bench run 6 (device printed UNKNOWN, 67 entries): the bit pattern reads as address 0x00 / command 0x08.
const std::vector<uint16_t> kRun6 = {
    8872, 4540, 522, 598, 538, 586, 528, 598, 516, 658, 478, 618, 516, 632, 470, 656, 500, 568, 536, 1736, 504, 1740,
    500,  1744, 504, 1740, 498, 1746, 504, 1766, 472, 1746, 492, 1746, 514, 610, 504, 624, 500, 594, 530, 1738, 502,
    596,  528,  598,  526,  674,  502,  544,  580,  1670, 538, 1726, 502, 1714, 536, 590, 546, 1696, 544, 1730, 500,
    1716, 534,  1738, 500};

// Bench run 4: header OK, two bits (cmd bits 0 and 1) with a short mark and a long space.
const std::vector<uint16_t> kRun4 = {
    8904, 4560, 492, 604, 552, 570, 524, 604, 520, 598, 548, 576, 580, 542, 572, 554, 518, 628, 496, 1724, 536, 1678,
    550,  1748, 490, 1724, 526, 1772, 476, 1714, 546, 1722, 518, 1698, 438, 716, 284, 840, 502, 624, 502, 1744, 504,
    644,  480,  620,  514,  580,  556,  596,  610,  1616, 528, 1706, 544, 1722, 506, 620, 494, 1774, 476, 1698, 552,
    1708, 530,  1716, 524};

std::vector<uint16_t> perfectNec() {
  IRsendTest tx(kPin);
  tx.begin();
  tx.send(NEC, tx.encodeNEC(0x00, 0x08), 32, 0);
  tx.makeDecodeResult();
  std::vector<uint16_t> out;
  for (uint16_t i = 1; i < tx.capture.rawlen; ++i) out.push_back(tx.capture.rawbuf[i] * kRawTick);
  return out;
}

bool decodesAsExpected(const std::vector<uint16_t>& us, bool trailingGap, uint8_t tolerance, int* typeOut = nullptr,
                       uint16_t* bitsOut = nullptr) {
  Replay r(us, trailingGap);
  IRrecv rx(kPin);
  rx.setTolerance(tolerance);
  const bool ok = rx.decode(&r.res);
  if (typeOut) *typeOut = r.res.decode_type;
  if (bitsOut) *bitsOut = r.res.bits;
  return ok && r.res.decode_type == NEC && r.res.address == 0x00 && r.res.command == 0x08;
}

}  // namespace

TEST_CASE("field captures: the replay harness decodes a perfect library-generated frame") {
  const std::vector<uint16_t> perfect = perfectNec();
  CHECK(decodesAsExpected(perfect, false, kTolerance));
}

// IRDB -> library cross-check: the real LG file converts to frames that the library itself encodes and decodes back.
// 0x20DF10EF is the widely published NEC word for LG TV "power" (address 0x04, command 0x08): taken from memory of
// public remote-code tables, not from a document read in this session.
TEST_CASE("IRDB LG file: converted frames round-trip through the library and match the known LG power word") {
  std::ifstream in(std::string(IRIS_FIXTURE_DIR) + "/irdb_LG.ir", std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  auto parsed = iris::parseFlipperIr(ss.str());
  REQUIRE(parsed.ok());
  REQUIRE(parsed.value().buttons.size() == 6);
  for (const auto& b : parsed.value().buttons) {
    IRsendTest tx(kPin);
    tx.begin();
    const iris::DecodedFrame& d = b.signal.initial.at(0).decoded;
    auto lf = iris::toLibraryFrame(tx, d);
    REQUIRE(lf.has_value());
    tx.send(lf->type, lf->data, lf->nbits, 0);
    tx.makeDecodeResult();
    IRrecv rx(kPin);
    REQUIRE(rx.decode(&tx.capture));
    auto back = iris::fromLibraryResult(tx.capture);
    REQUIRE(back.has_value());
    CHECK(back->address == d.address);
    CHECK(back->command == d.command);
    if (b.name == "Power") CHECK(lf->data == 0x20DF10EFu);
  }
}

// The PC build reproduces what the device printed (UNKNOWN, 34 bits) for both runs at the default 25 % tolerance.
// Run 6 carries the right bits but its marks are 30-60 us short; it is accepted from a 35 % tolerance up.
// Run 4 is not recoverable by tolerance alone (two bits have a 284 us mark).
TEST_CASE("field captures: run 6 is rejected at the default tolerance and accepted at 35 %") {
  for (bool gap : {false, true}) {
    int type = 0;
    uint16_t bits = 0;
    CHECK_FALSE(decodesAsExpected(kRun6, gap, kTolerance, &type, &bits));
    CHECK(type == UNKNOWN);
    CHECK(bits == 34);  // same count the device printed
    CHECK_FALSE(decodesAsExpected(kRun6, gap, 30));
    CHECK(decodesAsExpected(kRun6, gap, 35));
    CHECK(decodesAsExpected(kRun6, gap, 50));
  }
}

TEST_CASE("field captures: run 4 is not decoded as sent at any tolerance up to 50 %") {
  for (bool gap : {false, true})
    for (int tol : {25, 35, 50}) CHECK_FALSE(decodesAsExpected(kRun4, gap, static_cast<uint8_t>(tol)));
}
