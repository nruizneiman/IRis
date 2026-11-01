#include <doctest.h>

#include "app/RemoteLibrary.h"
#include "app/SignalBuilder.h"
#include "domain/Limits.h"

using namespace iris;

TEST_CASE("duplicate remote: copies buttons and signals with fresh ids, source untouched") {
  RemoteLibrary lib;
  Id cat = lib.addCategory("TV").value();
  Id src = lib.addRemote("Sala", cat).value();
  Id power = lib.addButton(src, "Power").value();
  lib.addButton(src, "Mute");
  REQUIRE(lib.setButtonSignal(src, power, buildDecodedSignal(Protocol::Nec, 0, 8, 32).value()).ok());
  lib.markClean();
  const Id nextBefore = lib.data().nextId;

  auto copy = lib.duplicateRemote(src, "  Sala 2 ");
  REQUIRE(copy.ok());
  CHECK(lib.dirty());
  const Remote* c = lib.findRemote(copy.value());
  REQUIRE(c != nullptr);
  CHECK(c->name == "Sala 2");
  CHECK(c->categoryId == cat);
  REQUIRE(c->buttons.size() == 2);
  CHECK(c->buttons[0].name == "Power");
  CHECK(c->buttons[0].signal.has_value());
  CHECK_FALSE(c->buttons[1].signal.has_value());
  CHECK(c->id >= nextBefore);
  for (const Button& b : c->buttons) CHECK(b.id >= nextBefore);
  CHECK(c->buttons[0].id != c->buttons[1].id);

  // Editing the copy does not affect the source.
  REQUIRE(lib.clearButtonSignal(copy.value(), c->buttons[0].id).ok());
  CHECK(lib.findRemote(src)->buttons[0].signal.has_value());
  CHECK(validateLibraryData(lib.data()).ok());
}

TEST_CASE("duplicate remote: names, limits and missing sources are refused") {
  RemoteLibrary lib;
  Id cat = lib.addCategory("TV").value();
  Id src = lib.addRemote("Sala", cat).value();
  CHECK(lib.duplicateRemote(999, "X").status().code == Err::NotFound);
  CHECK(lib.duplicateRemote(src, "sala").status().code == Err::AlreadyExists);
  CHECK(lib.duplicateRemote(src, "").status().code == Err::InvalidArgument);
  for (size_t i = 1; i < limits::kMaxRemotes; ++i) REQUIRE(lib.addRemote("R" + std::to_string(i), cat).ok());
  CHECK(lib.duplicateRemote(src, "Una mas").status().code == Err::LimitReached);
}

TEST_CASE("duplicate remote: the raw budget of the whole library still applies") {
  RemoteLibrary lib;
  Id cat = lib.addCategory("TV").value();
  IrSignal big = buildRawSignal(std::vector<uint16_t>(limits::kMaxRawEntries, 500)).value();
  const size_t perRemote = limits::kMaxRawEntriesPerRemote / limits::kMaxRawEntries;
  Id first = lib.addRemote("R0", cat).value();
  for (size_t i = 0; i < perRemote; ++i) {
    Id b = lib.addButton(first, "B" + std::to_string(i)).value();
    REQUIRE(lib.setButtonSignal(first, b, big).ok());
  }
  size_t copies = 0;
  for (int i = 1; i < 12; ++i) {
    if (lib.duplicateRemote(first, "C" + std::to_string(i)).ok()) ++copies;
  }
  CHECK(copies >= 1);
  CHECK(copies < 11);  // it stopped at the budget
  CHECK(lib.totalRawEntries() <= limits::kMaxTotalRawEntries);
}
