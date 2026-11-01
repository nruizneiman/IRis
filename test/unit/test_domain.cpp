#include <doctest.h>

#include "Fixtures.h"
#include "app/ExplorationPlan.h"
#include "app/SignalBuilder.h"
#include "domain/Limits.h"
#include "domain/Naming.h"
#include "domain/SignalValidation.h"
#include "persistence/Crc32.h"

using namespace iris;

TEST_CASE("crc32 matches the standard check value") {
  CHECK(crc32("123456789") == 0xCBF43926u);
  CHECK(crc32("") == 0u);
}

TEST_CASE("names: trimming, length, control characters, UTF-8") {
  CHECK(trimName("  TV sala \t") == "TV sala");
  CHECK(validateName("Televisión").ok());
  CHECK(validateName("Ñandú").ok());
  CHECK(validateName("").code == Err::InvalidArgument);
  CHECK(validateName(std::string(limits::kMaxNameBytes + 1, 'a')).code == Err::OutOfRange);
  CHECK(validateName(std::string(limits::kMaxNameBytes, 'a')).ok());
  CHECK(validateName("a\nb").code == Err::InvalidArgument);
  CHECK(validateName(std::string("bad\xC3")).code == Err::InvalidArgument);   // truncated sequence
  CHECK(validateName(std::string("bad\xFF")).code == Err::InvalidArgument);
  CHECK(namesEqual("Power", "pOWER"));
  CHECK_FALSE(namesEqual("Power", "Powers"));
}

TEST_CASE("decoded frame validation per protocol") {
  auto ok = [](Protocol p, uint32_t a, uint32_t c, uint16_t bits, bool t = false) {
    return buildDecodedSignal(p, a, c, bits, t);
  };
  CHECK(ok(Protocol::Nec, 0x00, 0x08, 32).ok());
  CHECK(ok(Protocol::Nec, 0xFFFF, 0xFF, 32).ok());           // extended NEC address
  CHECK(ok(Protocol::Nec, 0x10000, 0x00, 32).status().code == Err::OutOfRange);
  CHECK(ok(Protocol::Nec, 0x00, 0x100, 32).status().code == Err::OutOfRange);
  CHECK(ok(Protocol::Nec, 0x00, 0x00, 16).status().code == Err::OutOfRange);   // wrong bit count
  CHECK(ok(Protocol::Sony, 0x1F, 0x7F, 12).ok());
  CHECK(ok(Protocol::Sony, 0x20, 0x00, 12).status().code == Err::OutOfRange);  // 5-bit address
  CHECK(ok(Protocol::Sony, 0x1F, 0x7FFF, 20).ok());   // 20 bit: 5-bit address, 7+8 command bits
  CHECK(ok(Protocol::Sony, 0x20, 0x00, 20).status().code == Err::OutOfRange);
  CHECK(ok(Protocol::Sony, 0x00, 0x8000, 20).status().code == Err::OutOfRange);
  CHECK(ok(Protocol::Sony, 0x00, 0x80, 12).status().code == Err::OutOfRange);
  CHECK(ok(Protocol::Rc5, 0x1F, 0x3F, 12, true).ok());
  CHECK(ok(Protocol::Rc5, 0x1F, 0x40, 12).status().code == Err::OutOfRange);
  CHECK(ok(Protocol::Rc5, 0x1F, 0x40, 13).ok());
  CHECK(ok(Protocol::Nec, 0, 0, 32, true).status().code == Err::InvalidArgument);  // no toggle in NEC
  CHECK(ok(Protocol::Unknown, 0, 0, 32).status().code == Err::InvalidArgument);
}

TEST_CASE("manual decoded signal is Recognized, Manual, one initial frame") {
  auto r = buildDecodedSignal(Protocol::Samsung, 0x07, 0x02, 32);
  REQUIRE(r.ok());
  CHECK(r.value().provenance == Provenance::Manual);
  CHECK(r.value().recognition == Recognition::Recognized);
  CHECK(r.value().initial.size() == 1);
  CHECK(validateSignal(r.value()).ok());
}

TEST_CASE("raw signal validation") {
  CHECK(buildRawSignal(testkit::syntheticNecFrame(0xF708FF00u), 38000).ok());
  CHECK(buildRawSignal({}, 0).status().code == Err::InvalidArgument);
  CHECK(buildRawSignal({500}, 0).status().code == Err::InvalidArgument);          // needs mark+space
  CHECK(buildRawSignal({500, 0, 500}, 0).status().code == Err::InvalidArgument);  // zero duration
  CHECK(buildRawSignal({500, 500}, 5000).status().code == Err::OutOfRange);       // carrier too low
  CHECK(buildRawSignal({500, 500}, 0).ok());                                      // unknown carrier
  std::vector<uint16_t> tooLong(limits::kMaxRawEntries + 1, 500);
  CHECK(buildRawSignal(tooLong, 0).status().code == Err::LimitReached);
  std::vector<uint16_t> maxLen(limits::kMaxRawEntries, 500);
  CHECK(buildRawSignal(maxLen, 0).ok());
  auto r = buildRawSignal({500, 500}, 0);
  CHECK(r.value().recognition == Recognition::Unknown);
}

TEST_CASE("signal validation: structure, repeats and invalid recognition") {
  IrSignal s = testkit::syntheticNecSignalWithRepeats();
  CHECK(validateSignal(s).ok());
  CHECK(rawEntryCount(s) > 0);

  IrSignal bad = s;
  bad.recognition = Recognition::Invalid;
  CHECK_FALSE(validateSignal(bad).ok());

  bad = s;
  bad.initial.clear();
  bad.repeat.clear();
  CHECK_FALSE(validateSignal(bad).ok());

  bad = s;
  bad.repeat.clear();  // repeatCount > 0 with nothing to repeat
  CHECK_FALSE(validateSignal(bad).ok());

  bad = s;
  bad.repeatCount = limits::kMaxRepeatCount + 1;
  CHECK(validateSignal(bad).code == Err::OutOfRange);

  bad = s;
  bad.initial.resize(limits::kMaxFramesPerList + 1, s.initial[0]);
  CHECK(validateSignal(bad).code == Err::LimitReached);
}

TEST_CASE("capture analysis: empty, short, noisy, truncated, plausible") {
  auto a = analyzeCapture(testkit::emptyCapture());
  CHECK(a.verdict == Recognition::Invalid);
  CHECK(a.quality.empty);
  CHECK_FALSE(a.status.ok());

  a = analyzeCapture({500, 500, 500});
  CHECK(a.verdict == Recognition::Invalid);
  CHECK_FALSE(a.status.ok());

  a = analyzeCapture(testkit::syntheticNoiseCapture());
  CHECK(a.quality.possibleNoise);
  CHECK(a.status.ok());  // kept for inspection; the UI decides what to do with a noisy flag

  a = analyzeCapture(testkit::syntheticNecFrame(0xF708FF00u));
  CHECK_FALSE(a.quality.possibleNoise);
  CHECK_FALSE(a.quality.truncated);
  CHECK(a.verdict == Recognition::Unknown);  // decoding is the adapter's job

  a = analyzeCapture(std::vector<uint16_t>(limits::kMaxRawEntries, 500));
  CHECK(a.quality.truncated);

  a = analyzeCapture(testkit::syntheticUnknownPattern());
  CHECK(a.status.ok());
}

TEST_CASE("exploration plan: validation and hard limits") {
  ExplorationPlan p;
  p.protocol = Protocol::Nec;
  p.bits = 32;
  p.addressFirst = 0;
  p.addressLast = 0;
  p.commandFirst = 0;
  p.commandLast = 255;
  p.maxTransmissions = 100;
  p.gapMs = 500;
  CHECK(validateExplorationPlan(p).ok());
  CHECK(explorationCandidateCount(p) == 100);  // capped by maxTransmissions (range is 256)

  ExplorationPlan q = p;
  q.maxTransmissions = limits::kExplorationHardMaxTransmissions + 1;
  CHECK(validateExplorationPlan(q).code == Err::LimitReached);
  CHECK(explorationCandidateCount(q) == 0);

  q = p; q.maxTransmissions = 0;
  CHECK_FALSE(validateExplorationPlan(q).ok());
  q = p; q.gapMs = limits::kExplorationMinGapMs - 1;
  CHECK(validateExplorationPlan(q).code == Err::OutOfRange);
  q = p; q.gapMs = limits::kExplorationMaxGapMs + 1;
  CHECK(validateExplorationPlan(q).code == Err::OutOfRange);
  q = p; q.commandFirst = 10; q.commandLast = 5;
  CHECK(validateExplorationPlan(q).code == Err::InvalidArgument);
  q = p; q.commandLast = 256;
  CHECK(validateExplorationPlan(q).code == Err::OutOfRange);
  q = p; q.protocol = Protocol::Unknown;
  CHECK_FALSE(validateExplorationPlan(q).ok());
  q = p; q.bits = 12;
  CHECK_FALSE(validateExplorationPlan(q).ok());
}

TEST_CASE("exploration plan: confirmation threshold and enumeration without repeats") {
  ExplorationPlan p;
  p.protocol = Protocol::Sony;
  p.bits = 12;
  p.addressFirst = 1; p.addressLast = 2;
  p.commandFirst = 10; p.commandLast = 12;
  p.maxTransmissions = 500;
  p.gapMs = 300;
  REQUIRE(validateExplorationPlan(p).ok());
  CHECK(explorationCandidateCount(p) == 6);
  CHECK_FALSE(explorationNeedsConfirmation(p));

  const uint32_t expected[6][2] = {{1, 10}, {1, 11}, {1, 12}, {2, 10}, {2, 11}, {2, 12}};
  for (uint32_t i = 0; i < 6; ++i) {
    auto c = explorationCandidateAt(p, i);
    REQUIRE(c.ok());
    CHECK(c.value().address == expected[i][0]);
    CHECK(c.value().command == expected[i][1]);
  }
  CHECK(explorationCandidateAt(p, 6).status().code == Err::OutOfRange);

  ExplorationPlan big = p;
  big.addressFirst = 0; big.addressLast = 31;
  big.commandFirst = 0; big.commandLast = 127;
  big.maxTransmissions = 1000;
  CHECK(explorationCandidateCount(big) == 1000);
  CHECK(explorationNeedsConfirmation(big));
  CHECK(explorationCandidateAt(big, 999).ok());
  CHECK_FALSE(explorationCandidateAt(big, 1000).ok());
}
