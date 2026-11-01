#include <doctest.h>

#include "FakeClock.h"
#include "MemoryBlobStore.h"
#include "app/Boot.h"
#include "app/SignalBuilder.h"

using namespace iris;
using namespace iris::testkit;

namespace {
struct BootRig {
  FakeClock clock;
  MemoryBlobStore store;
  RingLogSink log{clock, 50};
  LibraryRepository repo{store};
  SettingsStore settingsStore{store};
  RemoteLibrary library;
  Settings settings;
  BootReport boot() { return loadPersistentData(store, repo, library, settingsStore, settings, log); }
};

RemoteLibrary sample() {
  RemoteLibrary lib;
  Id cat = lib.addCategory("TV").value();
  Id r = lib.addRemote("Sala", cat).value();
  Id b = lib.addButton(r, "Power").value();
  lib.setButtonSignal(r, b, buildDecodedSignal(Protocol::Nec, 0, 8, 32).value());
  return lib;
}
}  // namespace

TEST_CASE("boot: first start is empty and saving stays enabled") {
  BootRig r;
  BootReport rep = r.boot();
  CHECK(rep.persistEnabled);
  CHECK(rep.notice.empty());
  CHECK(r.library.data().remotes.empty());
}

TEST_CASE("boot: stored data and settings are loaded") {
  BootRig r;
  LibraryRepository other(r.store);
  REQUIRE(other.save(sample()).ok());
  Settings s;
  s.captureTimeoutSec = 30;
  REQUIRE(r.settingsStore.save(s).ok());
  BootReport rep = r.boot();
  CHECK(rep.persistEnabled);
  CHECK(rep.notice.empty());
  CHECK(r.library.data().remotes.size() == 1);
  CHECK(r.settings.captureTimeoutSec == 30);
}

TEST_CASE("boot: backup restore and lost remotes are announced, saving stays enabled") {
  BootRig r;
  {
    LibraryRepository other(r.store);
    RemoteLibrary lib = sample();
    REQUIRE(other.save(lib).ok());
    lib.addCategory("Extra");
    REQUIRE(other.save(lib).ok());
  }
  r.store.flipByte(LibraryRepository::indexKey(), 20);
  BootReport rep = r.boot();
  CHECK(rep.persistEnabled);
  CHECK(rep.notice == "Se usó el respaldo");
  CHECK(r.log.contains("restored from backup"));

  BootRig lost;
  {
    LibraryRepository other(lost.store);
    REQUIRE(other.save(sample()).ok());
  }
  for (const auto& k : lost.store.list()) {
    if (k.rfind("r", 0) == 0) lost.store.flipByte(k, 30);
  }
  BootReport rep2 = lost.boot();
  CHECK(rep2.persistEnabled);
  CHECK(rep2.notice == "Perdido: Sala");
  CHECK(lost.library.data().remotes.empty());
}

TEST_CASE("boot: both indexes damaged starts empty, keeps the bytes, and may save") {
  BootRig r;
  r.store.put(LibraryRepository::indexKey(), "garbage");
  r.store.put(LibraryRepository::backupIndexKey(), "garbage too");
  BootReport rep = r.boot();
  CHECK(rep.persistEnabled);
  CHECK(rep.notice == "Datos dañados: vacío");
  CHECK(r.store.read("badidx.irs").value() == "garbage");
}

TEST_CASE("boot: a read error or newer data disables saving and touches nothing") {
  BootRig r;
  {
    LibraryRepository other(r.store);
    REQUIRE(other.save(sample()).ok());
  }
  const auto filesBefore = r.store.list();
  r.store.setFailReads(true);
  BootReport rep = r.boot();
  CHECK_FALSE(rep.persistEnabled);
  CHECK(rep.notice == "No se pudo leer: sin guardar");
  r.store.setFailReads(false);
  CHECK(r.store.list() == filesBefore);  // nothing written, nothing deleted

  BootRig newer;
  newer.store.put(LibraryRepository::indexKey(), wrapEnvelope(kFormatVersion + 1, "{}"));
  BootReport rep2 = newer.boot();
  CHECK_FALSE(rep2.persistEnabled);
  CHECK(rep2.notice == "Datos de otra versión");
  CHECK_FALSE(newer.store.exists("badidx.irs"));
}

TEST_CASE("boot history: numbered, bounded, tolerant of damage") {
  MemoryBlobStore store;
  BootHistory history(store);
  CHECK(history.load().empty());
  for (int i = 0; i < 15; ++i) REQUIRE(history.record(i % 2 ? "BROWNOUT" : "encendido").ok());
  auto entries = history.load();
  REQUIRE(entries.size() == BootHistory::kMaxEntries);
  CHECK(entries.back().number == 15);
  CHECK(entries.front().number == 15 - BootHistory::kMaxEntries + 1);
  const auto lines = BootHistory::format(entries);
  CHECK(lines.front() == "#15 encendido");  // newest first
  CHECK(lines.back().rfind("#4 ", 0) == 0);

  // Damage: garbage lines are skipped, numbering continues from the last valid line.
  store.put(BootHistory::key(), "junk\n7 PANIC\n\nx y\n8 software\n");
  entries = history.load();
  REQUIRE(entries.size() == 2);
  CHECK(entries[0].reason == "PANIC");
  REQUIRE(history.record("WDT").ok());
  CHECK(history.load().back().number == 9);

  // Long or multi-line reasons cannot break the format.
  REQUIRE(history.record("a very long reason that keeps going and going").ok());
  REQUIRE(history.record("two" + std::string(1, 10) + "lines").ok());
  for (const auto& e : history.load()) CHECK(e.reason.find(char(10)) == std::string::npos);
  store.setFailReads(true);
  CHECK(history.load().empty());  // unreadable history is just empty, never an error
}
