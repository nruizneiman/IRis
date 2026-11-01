#include <doctest.h>

#include "Fixtures.h"
#include "app/RemoteLibrary.h"
#include "app/SignalBuilder.h"
#include "domain/Limits.h"

using namespace iris;

namespace {
IrSignal necSignal() { return buildDecodedSignal(Protocol::Nec, 0, 8, 32).value(); }
}  // namespace

TEST_CASE("categories: create, rename, delete, uniqueness, limits") {
  RemoteLibrary lib;
  auto tv = lib.addCategory("  TV ");
  REQUIRE(tv.ok());
  CHECK(lib.findCategory(tv.value())->name == "TV");  // trimmed
  CHECK(lib.dirty());

  CHECK(lib.addCategory("tv").status().code == Err::AlreadyExists);  // case-insensitive
  CHECK(lib.addCategory("").status().code == Err::InvalidArgument);

  auto audio = lib.addCategory("Audio");
  REQUIRE(audio.ok());
  CHECK(lib.renameCategory(audio.value(), "Sonido").ok());
  CHECK(lib.renameCategory(audio.value(), "TV").code == Err::AlreadyExists);
  CHECK(lib.renameCategory(audio.value(), "sonido").ok());  // renaming to its own name (case change)
  CHECK(lib.renameCategory(999, "X").code == Err::NotFound);

  CHECK(lib.removeCategory(audio.value()).ok());
  CHECK(lib.removeCategory(audio.value()).code == Err::NotFound);

  for (size_t i = 1; i < limits::kMaxCategories; ++i) {
    REQUIRE(lib.addCategory("C" + std::to_string(i)).ok());
  }
  CHECK(lib.addCategory("Overflow").status().code == Err::LimitReached);
}

TEST_CASE("category with remotes cannot be removed") {
  RemoteLibrary lib;
  Id cat = lib.addCategory("TV").value();
  Id remote = lib.addRemote("Samsung", cat).value();
  CHECK(lib.removeCategory(cat).code == Err::InUse);
  REQUIRE(lib.removeRemote(remote).ok());
  CHECK(lib.removeCategory(cat).ok());
}

TEST_CASE("remotes: create, rename, move, delete, validation") {
  RemoteLibrary lib;
  Id tv = lib.addCategory("TV").value();
  Id ac = lib.addCategory("Aire").value();

  CHECK(lib.addRemote("Sala", 999).status().code == Err::NotFound);
  auto r = lib.addRemote("Sala", tv);
  REQUIRE(r.ok());
  CHECK(lib.addRemote("sala", ac).status().code == Err::AlreadyExists);
  CHECK(lib.renameRemote(r.value(), "Living").ok());
  CHECK(lib.moveRemote(r.value(), ac).ok());
  CHECK(lib.findRemote(r.value())->categoryId == ac);
  CHECK(lib.moveRemote(r.value(), 999).code == Err::NotFound);
  CHECK(lib.moveRemote(999, ac).code == Err::NotFound);
  CHECK(lib.removeRemote(r.value()).ok());
  CHECK(lib.findRemote(r.value()) == nullptr);
  CHECK(lib.removeRemote(r.value()).code == Err::NotFound);
}

TEST_CASE("remote and button limits") {
  RemoteLibrary lib;
  Id cat = lib.addCategory("TV").value();
  for (size_t i = 0; i < limits::kMaxRemotes; ++i) {
    REQUIRE(lib.addRemote("R" + std::to_string(i), cat).ok());
  }
  CHECK(lib.addRemote("Extra", cat).status().code == Err::LimitReached);

  Id remote = lib.data().remotes[0].id;
  for (size_t i = 0; i < limits::kMaxButtonsPerRemote; ++i) {
    REQUIRE(lib.addButton(remote, "B" + std::to_string(i)).ok());
  }
  CHECK(lib.addButton(remote, "Extra").status().code == Err::LimitReached);
}

TEST_CASE("buttons: add, rename, delete, signal assignment") {
  RemoteLibrary lib;
  Id cat = lib.addCategory("TV").value();
  Id remote = lib.addRemote("Sala", cat).value();
  auto power = lib.addButton(remote, "Power");
  REQUIRE(power.ok());
  CHECK(lib.addButton(remote, "POWER").status().code == Err::AlreadyExists);
  CHECK(lib.addButton(999, "X").status().code == Err::NotFound);
  CHECK_FALSE(lib.findButton(remote, power.value())->signal.has_value());

  CHECK(lib.setButtonSignal(remote, power.value(), necSignal()).ok());
  REQUIRE(lib.findButton(remote, power.value())->signal.has_value());

  IrSignal invalid;  // no frames
  CHECK_FALSE(lib.setButtonSignal(remote, power.value(), invalid).ok());
  CHECK(lib.findButton(remote, power.value())->signal.has_value());  // old signal untouched

  CHECK(lib.setButtonSignal(remote, 999, necSignal()).code == Err::NotFound);
  CHECK(lib.clearButtonSignal(remote, power.value()).ok());
  CHECK_FALSE(lib.findButton(remote, power.value())->signal.has_value());

  CHECK(lib.renameButton(remote, power.value(), "Encender").ok());
  CHECK(lib.removeButton(remote, power.value()).ok());
  CHECK(lib.removeButton(remote, power.value()).code == Err::NotFound);
}

TEST_CASE("raw storage budgets: per remote and for the whole library, replacements are accounted") {
  RemoteLibrary lib;
  Id cat = lib.addCategory("TV").value();
  IrSignal big = buildRawSignal(std::vector<uint16_t>(limits::kMaxRawEntries, 500)).value();

  // Per remote: only floor(4000 / 1024) = 3 full-size signals fit.
  Id first = lib.addRemote("R0", cat).value();
  std::vector<Id> buttons;
  for (int i = 0; i < 4; ++i) buttons.push_back(lib.addButton(first, "B" + std::to_string(i)).value());
  const size_t perRemote = limits::kMaxRawEntriesPerRemote / limits::kMaxRawEntries;
  for (size_t i = 0; i < perRemote; ++i) REQUIRE(lib.setButtonSignal(first, buttons[i], big).ok());
  CHECK(lib.setButtonSignal(first, buttons[perRemote], big).code == Err::LimitReached);
  CHECK(lib.setButtonSignal(first, buttons[0], big).ok());  // replacing an existing one still works

  // Whole library: keep adding remotes until the total budget is reached.
  size_t stored = perRemote;
  for (int r = 1; r < 12; ++r) {
    Id remote = lib.addRemote("R" + std::to_string(r), cat).value();
    for (size_t i = 0; i < perRemote; ++i) {
      Id b = lib.addButton(remote, "B" + std::to_string(i)).value();
      Status st = lib.setButtonSignal(remote, b, big);
      if (st.ok()) {
        ++stored;
      } else {
        CHECK(st.code == Err::LimitReached);
      }
    }
  }
  CHECK(stored * limits::kMaxRawEntries <= limits::kMaxTotalRawEntries);
  CHECK(stored * limits::kMaxRawEntries + limits::kMaxRawEntries > limits::kMaxTotalRawEntries);  // it filled up
}

TEST_CASE("dirty flag") {
  RemoteLibrary lib;
  CHECK_FALSE(lib.dirty());
  Id cat = lib.addCategory("TV").value();
  CHECK(lib.dirty());
  lib.markClean();
  CHECK(lib.addCategory("TV").status().code == Err::AlreadyExists);  // failed op does not dirty
  CHECK_FALSE(lib.dirty());
  REQUIRE(lib.renameCategory(cat, "Tele").ok());
  CHECK(lib.dirty());
}

TEST_CASE("fromData rejects inconsistent libraries") {
  LibraryData d;
  d.nextId = 10;
  d.categories.push_back({1, "TV"});
  d.remotes.push_back(Remote{2, "Sala", 1, {}});
  CHECK(RemoteLibrary::fromData(d).ok());

  LibraryData bad = d;
  bad.remotes[0].categoryId = 7;  // dangling
  CHECK(RemoteLibrary::fromData(bad).status().code == Err::Corrupt);

  bad = d;
  bad.remotes[0].id = 1;  // duplicate id with the category
  CHECK(RemoteLibrary::fromData(bad).status().code == Err::Corrupt);

  bad = d;
  bad.nextId = 2;  // not above an existing id
  CHECK(RemoteLibrary::fromData(bad).status().code == Err::Corrupt);

  bad = d;
  bad.nextId = 0;
  CHECK(RemoteLibrary::fromData(bad).status().code == Err::Corrupt);

  bad = d;
  bad.categories[0].name = "";
  CHECK(RemoteLibrary::fromData(bad).status().code == Err::Corrupt);

  bad = d;
  bad.remotes[0].buttons.push_back(Button{3, "Power", IrSignal{}});  // signal without frames
  CHECK(RemoteLibrary::fromData(bad).status().code == Err::Corrupt);
}
