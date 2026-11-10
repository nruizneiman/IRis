#include <doctest.h>

#include <algorithm>
#include <set>
#include <tuple>

#include "FakeClock.h"
#include "FakeFileSource.h"
#include "FakeIrTransmitter.h"
#include "app/RemoteLibrary.h"
#include "app/RemoteView.h"
#include "app/SequenceRunner.h"
#include "app/Sequences.h"
#include "app/SignalBuilder.h"

using namespace iris;
using namespace iris::testkit;

namespace {

std::string irFile(std::initializer_list<std::pair<const char*, int>> buttons) {
  std::string t = "Filetype: IR signals file\nVersion: 1\n";
  for (const auto& b : buttons) {
    char line[160];
    std::snprintf(line, sizeof line, "#\nname: %s\ntype: parsed\nprotocol: NEC\naddress: 04 00 00 00\ncommand: %02X 00 00 00\n", b.first,
                  b.second);
    t += line;
  }
  return t;
}

IrSignal nec(uint32_t addr, uint32_t cmd) { return buildDecodedSignal(Protocol::Nec, addr, cmd, 32).value(); }

struct RunnerRig {
  FakeClock clock;
  FakeIrTransmitter tx{clock};
  TransmitService svc{tx};
  SequenceRunner runner{svc, clock};
  // Advances time in 10 ms steps, servicing the transmitter and the runner like the main loop does.
  void run(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += 10) {
      clock.advanceMs(10);
      svc.service();
      runner.service();
    }
  }
};

}  // namespace

TEST_CASE("remote view: a saved control and a card control look the same to the modes") {
  RemoteLibrary lib;
  const Id cat = lib.addCategory("TV").value();
  const Id rem = lib.addRemote("Sala", cat).value();
  const Id b = lib.addButton(rem, "Power").value();
  REQUIRE(lib.setButtonSignal(rem, b, nec(4, 8)).ok());
  lib.addButton(rem, "Vacio");
  auto saved = viewSavedRemote(lib, rem);
  REQUIRE(saved.ok());
  CHECK(saved.value().name == "Sala");
  CHECK_FALSE(saved.value().fromCard);
  CHECK(saved.value().buttons.size() == 2);
  CHECK(saved.value().buttons[0].signal.has_value());
  CHECK_FALSE(saved.value().buttons[1].signal.has_value());

  FakeFileSource card;
  card.add("/irdb/TVs/LG/LG_TV.ir", irFile({{"Power", 8}, {"Vol_up", 2}}));
  auto cv = viewCardRemote(card, "/irdb/TVs/LG/LG_TV.ir");
  REQUIRE(cv.ok());
  CHECK(cv.value().name == "LG TV");
  CHECK(cv.value().fromCard);
  CHECK(cv.value().buttons.size() == 2);
  CHECK(findPowerButton(cv.value(), PowerWanted::Off)->name == "Power");
}

TEST_CASE("remote view: a card control is not limited to the 40 buttons it used to be") {
  FakeFileSource card;
  std::string t = "Filetype: IR signals file\nVersion: 1\n";
  for (int i = 0; i < 300; ++i) {
    char line[160];
    std::snprintf(line, sizeof line, "#\nname: B%d\ntype: parsed\nprotocol: NEC\naddress: 04 00 00 00\ncommand: %02X 00 00 00\n", i, i & 0xFF);
    t += line;
  }
  card.add("/irdb/Big/Big.ir", t);
  auto v = viewCardRemote(card, "/irdb/Big/Big.ir");
  REQUIRE(v.ok());
  CHECK(v.value().buttons.size() == 300);
  CHECK(v.value().report.overCapacity == 0);
}

TEST_CASE("remote view: failures say why") {
  FakeFileSource card;
  card.add("/irdb/x/bad.ir", "not an IR file");
  CHECK(viewCardRemote(card, "/irdb/x/bad.ir").status().code == Err::Corrupt);
  CHECK(viewCardRemote(card, "/irdb/x/none.ir").status().code == Err::NotFound);
  card.setPresent(false);
  CHECK(viewCardRemote(card, "/irdb/x/bad.ir").status().code == Err::Unavailable);
}

TEST_CASE("library: a control may hold hundreds of buttons, within a total for the whole library") {
  RemoteLibrary lib;
  const Id cat = lib.addCategory("TV").value();
  const Id rem = lib.addRemote("Grande", cat).value();
  for (size_t i = 0; i < limits::kMaxButtonsPerRemote; ++i) {
    REQUIRE(lib.addButton(rem, "B" + std::to_string(i)).ok());
  }
  CHECK(lib.addButton(rem, "Uno mas").status().code == Err::LimitReached);
  // Fill the library up to the total, then one more button anywhere is refused.
  size_t n = 1;
  while (lib.totalButtons() + limits::kMaxButtonsPerRemote <= limits::kMaxTotalButtons) {
    const Id r = lib.addRemote("R" + std::to_string(n++), cat).value();
    for (size_t i = 0; i < limits::kMaxButtonsPerRemote; ++i) REQUIRE(lib.addButton(r, "B" + std::to_string(i)).ok());
  }
  const Id last = lib.addRemote("Ultimo", cat).value();
  size_t added = 0;
  while (lib.addButton(last, "X" + std::to_string(added)).ok()) ++added;
  CHECK(lib.totalButtons() == limits::kMaxTotalButtons);
}

TEST_CASE("power button: explicit off or on first, the toggle otherwise, nothing when there is none") {
  RemoteView v;
  v.buttons.push_back({"Vol+", nec(4, 2)});
  v.buttons.push_back({"Power", nec(4, 8)});
  CHECK(findPowerButton(v, PowerWanted::Off)->name == "Power");
  CHECK(findPowerButton(v, PowerWanted::On)->name == "Power");
  v.buttons.push_back({"Power_Off", nec(4, 9)});
  v.buttons.push_back({"POWER ON", nec(4, 10)});
  CHECK(findPowerButton(v, PowerWanted::Off)->name == "Power_Off");
  CHECK(findPowerButton(v, PowerWanted::On)->name == "POWER ON");
  RemoteView none;
  none.buttons.push_back({"Vol+", nec(4, 2)});
  CHECK(findPowerButton(none, PowerWanted::Off) == nullptr);
  RemoteView empty;
  empty.buttons.push_back({"Power", std::nullopt});
  CHECK(findPowerButton(empty, PowerWanted::Off) == nullptr);
}

TEST_CASE("loop: the chosen buttons repeat in order with the chosen pause, and stop on request") {
  RunnerRig r;
  RemoteView v;
  v.name = "Sala";
  v.buttons.push_back({"A", nec(1, 1)});
  v.buttons.push_back({"B", nec(1, 2)});
  v.buttons.push_back({"C", nec(1, 3)});
  REQUIRE(r.runner.start(std::make_unique<LoopSequence>(v, std::vector<size_t>{2, 0, 2}), 1000).ok());
  r.run(10000);
  CHECK(r.runner.running());
  CHECK(r.runner.sent() >= 6);
  CHECK(r.runner.sent() <= 10);  // 70 ms of frame + 1000 ms pause each, in ten seconds
  const auto& sent = r.tx.history();
  REQUIRE(sent.size() >= 4);
  auto cmd = [&](size_t i) { return sent[i].job.signal.initial[0].decoded.command; };
  CHECK(cmd(0) == 3);
  CHECK(cmd(1) == 1);
  CHECK(cmd(2) == 3);
  CHECK(cmd(3) == 3);  // the order starts again
  r.runner.stop();
  const uint32_t at = r.runner.sent();
  r.run(5000);
  CHECK(r.runner.sent() == at);
  CHECK(r.runner.state() == SequenceState::Stopped);
}

TEST_CASE("loop: never faster than the minimum pause, however short is asked") {
  RunnerRig r;
  RemoteView v;
  v.buttons.push_back({"A", nec(1, 1)});
  REQUIRE(r.runner.start(std::make_unique<LoopSequence>(v, std::vector<size_t>{0}), 0).ok());
  r.run(2000);
  CHECK(r.runner.gapMs() == SequenceRunner::kMinGapMs);
  CHECK(r.runner.sent() <= 2000 / (SequenceRunner::kMinGapMs + 70) + 1);
}

TEST_CASE("loop: a finite number of rounds ends by itself; a second start is refused while running") {
  RunnerRig r;
  RemoteView v;
  v.buttons.push_back({"A", nec(1, 1)});
  v.buttons.push_back({"B", nec(1, 2)});
  REQUIRE(r.runner.start(std::make_unique<LoopSequence>(v, std::vector<size_t>{0, 1}, 2), 200).ok());
  CHECK_FALSE(r.runner.start(std::make_unique<LoopSequence>(v, std::vector<size_t>{0}), 200).ok());
  r.run(5000);
  CHECK(r.runner.state() == SequenceState::Done);
  CHECK(r.runner.sent() == 4);
}

TEST_CASE("crazy: only buttons with a signal, only from the chosen control, repeatable with a seed") {
  RemoteView v;
  v.buttons.push_back({"Vol+", nec(1, 1)});
  v.buttons.push_back({"Vol-", nec(1, 2)});
  v.buttons.push_back({"Vacio", std::nullopt});
  v.buttons.push_back({"Canal+", nec(1, 3)});
  CrazySequence a(v, 42), b(v, 42);
  std::set<std::string> seen;
  int repeats = 0;
  std::string previous;
  for (int i = 0; i < 300; ++i) {
    SequenceItem x, y;
    REQUIRE(a.next(x));
    REQUIRE(b.next(y));
    CHECK(x.label == y.label);  // same seed, same run
    CHECK(x.label != "Vacio");
    seen.insert(x.label);
    if (x.label == previous) ++repeats;
    previous = x.label;
  }
  CHECK(seen.size() == 3);
  CHECK(repeats > 10);  // it sometimes presses the same button twice or three times in a row
  RemoteView none;
  none.buttons.push_back({"X", std::nullopt});
  CrazySequence c(none, 1);
  SequenceItem z;
  CHECK_FALSE(c.next(z));
}

TEST_CASE("crazy: the pause varies around the chosen one and is short between repeats of a button") {
  RemoteView v;
  v.buttons.push_back({"A", nec(1, 1)});
  v.buttons.push_back({"B", nec(1, 2)});
  CrazySequence c(v, 7);
  uint32_t lo = 100000, hi = 0;
  for (int i = 0; i < 200; ++i) {
    SequenceItem x;
    c.next(x);
    const uint32_t g = c.gapAfterMs(2000);
    if (g >= 1000) {
      lo = std::min(lo, g);
      hi = std::max(hi, g);
    } else {
      CHECK(g == SequenceRunner::kMinGapMs * 2);
    }
  }
  CHECK(lo >= 1000);
  CHECK(hi <= 3000);
  CHECK(hi - lo > 500);
}

TEST_CASE("Be Gone on a card folder: every .ir file below it, one power code each, repeated codes sent once") {
  FakeFileSource card;
  card.add("/irdb/TVs/LG/a.ir", irFile({{"Power", 8}, {"Vol_up", 2}}));
  card.add("/irdb/TVs/LG/b.ir", irFile({{"Power", 8}}));          // same code as a.ir: not sent twice
  card.add("/irdb/TVs/Sony/c.ir", irFile({{"Power", 9}}));
  card.add("/irdb/TVs/Sony/d.ir", irFile({{"Vol_up", 2}}));       // no power button: nothing to send
  card.add("/irdb/TVs/Sony/e.txt", "ignored");
  card.add("/irdb/TVs/Bad/f.ir", "garbage");                       // unreadable: skipped, the run goes on
  card.add("/irdb/ACs/g.ir", irFile({{"Power", 10}}));             // outside the chosen folder
  auto source = std::make_unique<CardFolderSource>(card, "/irdb/TVs");
  CardFolderSource* src = source.get();
  BeGoneSequence seq(std::move(source), PowerWanted::Off);
  std::vector<uint32_t> cmds;
  SequenceItem it;
  while (seq.next(it)) cmds.push_back(it.signal.initial[0].decoded.command);
  REQUIRE(cmds.size() == 2);
  CHECK(cmds[0] == 8);
  CHECK(cmds[1] == 9);
  CHECK(seq.duplicatesSkipped() == 1);
  CHECK(src->filesSkipped() == 1);
  CHECK(src->filesRead() == 4);
}

TEST_CASE("Be Gone on a saved category: only the controls of that category") {
  RemoteLibrary lib;
  const Id tv = lib.addCategory("TV").value();
  const Id ac = lib.addCategory("AC").value();
  for (auto [cat, name, cmd] : {std::tuple<Id, const char*, int>{tv, "A", 1}, {tv, "B", 2}, {ac, "C", 3}}) {
    const Id r = lib.addRemote(name, cat).value();
    const Id b = lib.addButton(r, "Power").value();
    REQUIRE(lib.setButtonSignal(r, b, nec(1, static_cast<uint32_t>(cmd))).ok());
  }
  BeGoneSequence seq(std::make_unique<SavedCategorySource>(lib, tv), PowerWanted::On);
  SequenceItem it;
  int n = 0;
  while (seq.next(it)) ++n;
  CHECK(n == 2);
}

TEST_CASE("Be Gone through the runner: sends the codes one after another and finishes") {
  RunnerRig r;
  FakeFileSource card;
  card.add("/irdb/TVs/a.ir", irFile({{"Power", 1}}));
  card.add("/irdb/TVs/b.ir", irFile({{"Power", 2}}));
  card.add("/irdb/TVs/c.ir", irFile({{"Power", 3}}));
  REQUIRE(r.runner.start(std::make_unique<BeGoneSequence>(std::make_unique<CardFolderSource>(card, "/irdb/TVs"), PowerWanted::Off), 500).ok());
  r.run(5000);
  CHECK(r.runner.state() == SequenceState::Done);
  CHECK(r.runner.sent() == 3);
}
