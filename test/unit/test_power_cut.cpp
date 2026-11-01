// Power-cut simulation: a save is interrupted before every possible write, the "device" restarts
// (a new repository on the same store) and must find a complete library: the old one or the new one,
// never an error and never a mix. IBlobStore::write() is atomic by contract, so every interruption point
// is "the Nth write did not happen". The atomic rename itself is only verifiable on the real flash.
#include <doctest.h>

#include "MemoryBlobStore.h"
#include "app/RemoteLibrary.h"
#include "app/SignalBuilder.h"
#include "persistence/LibraryRepository.h"

using namespace iris;

namespace {

std::string snapshot(const RemoteLibrary& lib) {
  std::string s = "next=" + std::to_string(lib.data().nextId) + "\n";
  for (const Category& c : lib.data().categories) s += "cat " + std::to_string(c.id) + " " + c.name + "\n";
  for (const Remote& r : lib.data().remotes) {
    s += "remote " + std::to_string(r.id) + " " + r.name + " cat=" + std::to_string(r.categoryId) + "\n";
    s += serializeRemoteFile(r).value() + "\n";
  }
  return s;
}

RemoteLibrary base() {
  RemoteLibrary lib;
  Id cat = lib.addCategory("TV").value();
  Id audio = lib.addCategory("Audio").value();
  for (const char* name : {"Sala", "Cuarto", "Ampli"}) {
    Id r = lib.addRemote(name, name[0] == 'A' ? audio : cat).value();
    Id b = lib.addButton(r, "Power").value();
    lib.setButtonSignal(r, b, buildDecodedSignal(Protocol::Nec, 0, 8, 32).value());
  }
  lib.markClean();
  return lib;
}

// A change that touches two remotes, adds one and renames a category.
void edit(RemoteLibrary& lib) {
  const Id sala = lib.data().remotes[0].id;
  lib.renameButton(sala, lib.data().remotes[0].buttons[0].id, "Encender");
  Id b = lib.addButton(lib.data().remotes[1].id, "Mute").value();
  lib.setButtonSignal(lib.data().remotes[1].id, b, buildRawSignal({560, 560, 560, 1690}, 38000).value());
  Id extra = lib.addRemote("Nuevo", lib.data().categories[0].id).value();
  lib.addButton(extra, "Uno");
  lib.renameCategory(lib.data().categories[1].id, "Sonido");
}

}  // namespace

TEST_CASE("power cut at every write position of an update leaves the old or the new library") {
  RemoteLibrary before = base();
  RemoteLibrary after = base();
  edit(after);
  const std::string oldText = snapshot(before), newText = snapshot(after);
  REQUIRE(oldText != newText);

  bool sawOld = false, sawNew = false;
  for (int allowed = 0; allowed < 40; ++allowed) {
    testkit::MemoryBlobStore store;
    {
      LibraryRepository repo(store);
      REQUIRE(repo.save(before).ok());
      REQUIRE(repo.save(before).ok());  // a backup generation exists too
      store.failWritesAfter(allowed);
      Status st = repo.save(after);
      store.failWritesAfter(-1);
      INFO("allowed writes before the cut: " << allowed << ", save " << (st.ok() ? "completed" : "interrupted"));

      LibraryRepository boot(store);  // restart
      auto r = boot.load();
      REQUIRE(r.ok());
      CHECK(r.value().lostRemotes.empty());
      const std::string got = snapshot(r.value().library);
      CHECK((got == oldText || got == newText));
      (got == newText ? sawNew : sawOld) = true;
      if (st.ok()) CHECK(got == newText);

      // After the restart a normal save must succeed and be complete.
      REQUIRE(boot.save(after).ok());
      LibraryRepository boot2(store);
      CHECK(snapshot(boot2.load().value().library) == newText);
    }
    // Stop once a whole save fits in the allowance.
    if (sawNew && allowed > 20) break;
  }
  CHECK(sawOld);   // the earliest cuts keep the old library
  CHECK(sawNew);   // the latest ones complete the new one
}

TEST_CASE("power cut during the very first save: empty or complete, never broken") {
  RemoteLibrary lib = base();
  const std::string text = snapshot(lib);
  for (int allowed = 0; allowed < 12; ++allowed) {
    testkit::MemoryBlobStore store;
    LibraryRepository repo(store);
    store.failWritesAfter(allowed);
    Status st = repo.save(lib);
    store.failWritesAfter(-1);
    INFO("allowed " << allowed);
    LibraryRepository boot(store);
    auto r = boot.load();
    REQUIRE(r.ok());
    if (st.ok()) {
      CHECK(snapshot(r.value().library) == text);
    } else {
      CHECK((r.value().source == LoadSource::Empty || snapshot(r.value().library) == text));
    }
  }
}

TEST_CASE("orphan files left by an interrupted save are removed by the next successful one") {
  RemoteLibrary lib = base();
  testkit::MemoryBlobStore store;
  LibraryRepository repo(store);
  REQUIRE(repo.save(lib).ok());
  edit(lib);
  store.failWritesAfter(2);  // some remote files get written, the index does not
  CHECK_FALSE(repo.save(lib).ok());
  store.failWritesAfter(-1);
  size_t remoteFiles = 0;
  for (const auto& k : store.list()) remoteFiles += k.rfind("r", 0) == 0 ? 1 : 0;
  CHECK(remoteFiles > 3);  // orphans exist

  REQUIRE(repo.save(lib).ok());
  REQUIRE(repo.save(lib).ok());
  remoteFiles = 0;
  for (const auto& k : store.list()) remoteFiles += k.rfind("r", 0) == 0 ? 1 : 0;
  CHECK(remoteFiles == lib.data().remotes.size());  // exactly one file per remote again
}
