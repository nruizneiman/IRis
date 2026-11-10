#include <doctest.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>

#include "ArduinoJson.h"
#include "alloc_probe.h"
#include "Fixtures.h"
#include "MemoryBlobStore.h"
#include "app/RemoteLibrary.h"
#include "app/SignalBuilder.h"
#include "domain/Limits.h"
#include "persistence/Crc32.h"
#include "persistence/LibraryRepository.h"
#include "persistence/LibrarySerializer.h"

using namespace iris;

namespace {

// Library covering every signal shape: decoded, raw, unknown, synthetic with repeats + original.
RemoteLibrary makeRichLibrary() {
  RemoteLibrary lib;
  Id tv = lib.addCategory("TV").value();
  Id audio = lib.addCategory("Audio").value();
  Id sala = lib.addRemote("Televisión sala", tv).value();
  Id amp = lib.addRemote("Ampli", audio).value();

  Id power = lib.addButton(sala, "Power").value();
  Id raw = lib.addButton(sala, "Raw").value();
  Id unk = lib.addButton(sala, "Unknown").value();
  Id rep = lib.addButton(sala, "Repeats").value();
  lib.addButton(sala, "Sin señal");  // button without signal
  Id vol = lib.addButton(amp, "Vol+").value();

  lib.setButtonSignal(sala, power, buildDecodedSignal(Protocol::Rc5, 5, 12, 12, true).value());
  lib.setButtonSignal(sala, raw, buildRawSignal(testkit::syntheticNecFrame(0xF708FF00u), 38000).value());
  IrSignal u = buildRawSignal(testkit::syntheticUnknownPattern(), 0).value();
  u.provenance = Provenance::Learned;
  u.quality.possibleNoise = true;
  u.note = "nota con \"comillas\" y ñ";
  lib.setButtonSignal(sala, unk, u);
  lib.setButtonSignal(sala, rep, testkit::syntheticNecSignalWithRepeats());
  lib.setButtonSignal(amp, vol, buildDecodedSignal(Protocol::Sony, 0x10, 0x12, 12).value());
  lib.markClean();
  return lib;
}

// Everything that defines the library, as text: used to compare "same library" exactly.
std::string snapshot(const RemoteLibrary& lib) {
  std::string s = "next=" + std::to_string(lib.data().nextId) + "\n";
  for (const Category& c : lib.data().categories) s += "cat " + std::to_string(c.id) + " " + c.name + "\n";
  for (const Remote& r : lib.data().remotes) {
    s += "remote " + std::to_string(r.id) + " " + r.name + " cat=" + std::to_string(r.categoryId) + "\n";
    s += serializeRemoteFile(r).value();
    s += "\n";
  }
  return s;
}

std::string fixture(const char* name) {
  std::ifstream in(std::string(IRIS_FIXTURE_DIR) + "/" + name, std::ios::binary);
  REQUIRE_MESSAGE(in.good(), name);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

size_t countKeys(const testkit::MemoryBlobStore& s, const std::string& prefix) {
  size_t n = 0;
  for (const auto& k : s.list()) n += k.rfind(prefix, 0) == 0 ? 1 : 0;
  return n;
}

}  // namespace

// ------------------------------------------------------------------ envelope and files

TEST_CASE("remote file round-trips every signal shape and is stable") {
  RemoteLibrary lib = makeRichLibrary();
  const Remote& sala = lib.data().remotes[0];
  auto text = serializeRemoteFile(sala);
  REQUIRE(text.ok());
  auto back = deserializeRemoteFile(text.value());
  REQUIRE(back.ok());
  CHECK(back.value().id == sala.id);
  REQUIRE(back.value().buttons.size() == sala.buttons.size());
  CHECK(serializeRemoteFile(back.value()).value() == text.value());  // lossless and deterministic

  const Button& rep = back.value().buttons[3];
  REQUIRE(rep.signal.has_value());
  CHECK(rep.signal->provenance == Provenance::Synthetic);
  CHECK(rep.signal->repeatCount == 3);
  CHECK(rep.signal->repeatGapUs == 40000);
  REQUIRE(rep.signal->originalCapture.has_value());  // RAW kept next to the decoded frame
  CHECK(rep.signal->originalCapture->durationsUs == testkit::syntheticNecFrame(0xF708FF00u));
  const Button& unk = back.value().buttons[2];
  CHECK(unk.signal->quality.possibleNoise);
  CHECK(unk.signal->note == "nota con \"comillas\" y ñ");
  CHECK(back.value().buttons[0].signal->initial[0].decoded.toggle);
  CHECK_FALSE(back.value().buttons[4].signal.has_value());
}

TEST_CASE("raw durations are stored as hex text, default fields are omitted") {
  RemoteLibrary lib = makeRichLibrary();
  const std::string text = serializeRemoteFile(lib.data().remotes[0]).value();
  CHECK(text.find("\"raw\":\"") != std::string::npos);   // hex string, not a number array
  CHECK(text.find("\"raw\":[") == std::string::npos);
  CHECK(text.find("\"rcount\"") != std::string::npos);   // the one signal with repeats keeps it
  const std::string amp = serializeRemoteFile(lib.data().remotes[1]).value();
  CHECK(amp.find("rcount") == std::string::npos);        // plain signals carry no default fields
  CHECK(amp.find("\"q\"") == std::string::npos);
}

TEST_CASE("corruption of a file is detected, never silently accepted") {
  const std::string good = serializeRemoteFile(makeRichLibrary().data().remotes[0]).value();

  SUBCASE("flipped byte in body") {
    std::string bad = good;
    bad[good.size() - 5] ^= 0x01;
    CHECK(deserializeRemoteFile(bad).status().code == Err::Corrupt);
  }
  SUBCASE("truncated / extended file") {
    CHECK(deserializeRemoteFile(good.substr(0, good.size() / 2)).status().code == Err::Corrupt);
    CHECK(deserializeRemoteFile(good + "xx").status().code == Err::Corrupt);
  }
  SUBCASE("empty / no header / wrong magic") {
    CHECK(deserializeRemoteFile("").status().code == Err::Corrupt);
    CHECK(deserializeRemoteFile("{}").status().code == Err::Corrupt);
    CHECK(deserializeRemoteFile("NOPE 2 00000000 2\n{}").status().code == Err::Corrupt);
  }
  SUBCASE("valid envelope but structurally invalid content") {
    CHECK(deserializeRemoteFile(wrapEnvelope(kFormatVersion, "{\"id\":1}")).status().code == Err::Corrupt);
    CHECK(deserializeRemoteFile(wrapEnvelope(kFormatVersion, "not json")).status().code == Err::Corrupt);
    // RAW hex of the wrong length / with non-hex digits
    CHECK(deserializeRemoteFile(wrapEnvelope(kFormatVersion,
          "{\"id\":1,\"buttons\":[{\"id\":2,\"name\":\"a\",\"sig\":{\"prov\":\"manual\",\"rec\":\"unknown\","
          "\"init\":[{\"hz\":0,\"raw\":\"12345\"}]}}]}")).status().code == Err::Corrupt);
    CHECK(deserializeRemoteFile(wrapEnvelope(kFormatVersion,
          "{\"id\":1,\"buttons\":[{\"id\":2,\"name\":\"a\",\"sig\":{\"prov\":\"manual\",\"rec\":\"unknown\","
          "\"init\":[{\"hz\":0,\"raw\":\"12zz\"}]}}]}")).status().code == Err::Corrupt);
  }
  SUBCASE("an index is not a remote file and vice versa") {
    IndexData idx;
    const std::string indexText = serializeIndex(idx).value();
    CHECK(deserializeRemoteFile(indexText).status().code == Err::Corrupt);
    CHECK(deserializeIndex(good).status().code == Err::Corrupt);
  }
}

TEST_CASE("a file from a newer firmware is refused with UnsupportedVersion") {
  const std::string text = wrapEnvelope(kFormatVersion + 1, "{}");
  CHECK(openEnvelope(text).status().code == Err::UnsupportedVersion);
  CHECK(deserializeIndex(text).status().code == Err::UnsupportedVersion);
  CHECK(deserializeRemoteFile(text).status().code == Err::UnsupportedVersion);
}

// ------------------------------------------------------------------ repository

TEST_CASE("repository: first boot yields an empty library") {
  testkit::MemoryBlobStore store;
  LibraryRepository repo(store);
  auto r = repo.load();
  REQUIRE(r.ok());
  CHECK(r.value().source == LoadSource::Empty);
  CHECK(r.value().library.data().remotes.empty());
}

TEST_CASE("repository: save then load restores the library exactly") {
  testkit::MemoryBlobStore store;
  LibraryRepository repo(store);
  RemoteLibrary lib = makeRichLibrary();
  REQUIRE(repo.save(lib).ok());
  LibraryRepository fresh(store);  // a new boot
  auto r = fresh.load();
  REQUIRE(r.ok());
  CHECK(r.value().source == LoadSource::Primary);
  CHECK(r.value().lostRemotes.empty());
  CHECK(snapshot(r.value().library) == snapshot(lib));
}

TEST_CASE("repository: an empty library can be saved and loaded") {
  testkit::MemoryBlobStore store;
  LibraryRepository repo(store);
  REQUIRE(repo.save(RemoteLibrary{}).ok());
  LibraryRepository fresh(store);
  auto r = fresh.load();
  REQUIRE(r.ok());
  CHECK(r.value().source == LoadSource::Primary);
  CHECK(r.value().library.data().categories.empty());
}

TEST_CASE("repository: only changed remotes are rewritten and old generations are collected") {
  testkit::MemoryBlobStore store;
  LibraryRepository repo(store);
  RemoteLibrary lib = makeRichLibrary();
  REQUIRE(repo.save(lib).ok());
  const uint32_t gen1 = repo.generation();
  CHECK(countKeys(store, "r") == 2);  // one file per remote

  const Id ampId = lib.data().remotes[1].id;
  lib.renameButton(ampId, lib.data().remotes[1].buttons[0].id, "Vol-");
  REQUIRE(repo.save(lib).ok());
  CHECK(repo.generation() == gen1 + 1);
  // The unchanged remote keeps its file; the changed one has a new file; the old one is kept as backup.
  CHECK(countKeys(store, "r") == 3);
  REQUIRE(repo.save(lib).ok());           // nothing changed: no remote file written, backup refreshed
  CHECK(countKeys(store, "r") == 2);      // the superseded generation is collected now
}

TEST_CASE("repository: corrupt index falls back to the backup index") {
  testkit::MemoryBlobStore store;
  LibraryRepository repo(store);
  RemoteLibrary lib = makeRichLibrary();
  REQUIRE(repo.save(lib).ok());             // generation 1
  const std::string gen1 = snapshot(lib);
  lib.addCategory("Extra");
  REQUIRE(repo.save(lib).ok());             // generation 2; generation 1 is the backup

  store.flipByte(LibraryRepository::indexKey(), 20);
  LibraryRepository boot(store);
  auto r = boot.load();
  REQUIRE(r.ok());
  CHECK(r.value().source == LoadSource::Backup);
  CHECK(r.value().primaryStatus.code == Err::Corrupt);
  CHECK(snapshot(r.value().library) == gen1);
}

TEST_CASE("repository: a damaged index never replaces a good backup") {
  testkit::MemoryBlobStore store;
  LibraryRepository repo(store);
  RemoteLibrary lib = makeRichLibrary();
  REQUIRE(repo.save(lib).ok());
  REQUIRE(repo.save(lib).ok());
  store.flipByte(LibraryRepository::indexKey(), 20);
  const std::string backupBefore = store.read(LibraryRepository::backupIndexKey()).value();

  lib.addCategory("Tras la corrupción");
  REQUIRE(repo.save(lib).ok());
  CHECK(store.read(LibraryRepository::backupIndexKey()).value() == backupBefore);  // not rotated from garbage
  LibraryRepository boot(store);
  CHECK(boot.load().value().source == LoadSource::Primary);
}

TEST_CASE("repository: a damaged remote file loses only that remote and keeps its bytes") {
  testkit::MemoryBlobStore store;
  LibraryRepository repo(store);
  RemoteLibrary lib = makeRichLibrary();
  REQUIRE(repo.save(lib).ok());
  std::string victimKey;
  for (const auto& k : store.list()) {
    if (k.rfind("r", 0) == 0 && k.find("g") != std::string::npos &&
        k.rfind("r" + std::to_string(lib.data().remotes[0].id) + "g", 0) == 0) {
      victimKey = k;
    }
  }
  REQUIRE_FALSE(victimKey.empty());
  store.flipByte(victimKey, 40);

  LibraryRepository boot(store);
  auto r = boot.load();
  REQUIRE(r.ok());
  REQUIRE(r.value().lostRemotes.size() == 1);
  CHECK(r.value().lostRemotes[0] == "Televisión sala");
  CHECK(r.value().library.data().remotes.size() == 1);          // the other remote is intact
  CHECK(r.value().library.data().remotes[0].name == "Ampli");
  CHECK(store.exists("bad" + std::to_string(lib.data().remotes[0].id) + ".irs"));  // bytes kept for inspection
}

TEST_CASE("repository: a missing remote file is reported, a stale one (wrong CRC) too") {
  testkit::MemoryBlobStore store;
  LibraryRepository repo(store);
  RemoteLibrary lib = makeRichLibrary();
  REQUIRE(repo.save(lib).ok());
  for (const auto& k : store.list()) {
    if (k.rfind("r", 0) == 0 && k.find(".irs") != std::string::npos && k.find("g") != std::string::npos) {
      if (k.rfind("r" + std::to_string(lib.data().remotes[0].id) + "g", 0) == 0) store.remove(k);
      else store.put(k, serializeRemoteFile(lib.data().remotes[0]).value());  // wrong remote's valid file
    }
  }
  LibraryRepository boot(store);
  auto r = boot.load();
  REQUIRE(r.ok());
  CHECK(r.value().lostRemotes.size() == 2);
  CHECK(r.value().library.data().remotes.empty());
}

TEST_CASE("repository: both indexes unreadable is an error and leaves every file alone") {
  testkit::MemoryBlobStore store;
  LibraryRepository repo(store);
  REQUIRE(repo.save(makeRichLibrary()).ok());
  REQUIRE(repo.save(makeRichLibrary()).ok());
  store.put(LibraryRepository::indexKey(), "garbage");
  store.put(LibraryRepository::backupIndexKey(), "garbage too");
  const size_t files = store.list().size();
  LibraryRepository boot(store);
  auto r = boot.load();
  CHECK_FALSE(r.ok());
  CHECK(r.status().code == Err::Corrupt);
  CHECK(store.list().size() == files);
  CHECK(store.read(LibraryRepository::indexKey()).value() == "garbage");
}

TEST_CASE("repository: transient read failures are not reported as corruption") {
  testkit::MemoryBlobStore store;
  LibraryRepository repo(store);
  REQUIRE(repo.save(makeRichLibrary()).ok());
  store.setFailReads(true);
  LibraryRepository boot(store);
  auto r = boot.load();
  CHECK_FALSE(r.ok());
  CHECK(r.status().code == Err::StorageFailure);  // NOT Corrupt: the caller must not discard or overwrite
}

TEST_CASE("repository: data from a newer firmware stops the load without falling back to the backup") {
  testkit::MemoryBlobStore store;
  LibraryRepository repo(store);
  RemoteLibrary lib = makeRichLibrary();
  REQUIRE(repo.save(lib).ok());
  REQUIRE(repo.save(lib).ok());
  store.put(LibraryRepository::indexKey(), wrapEnvelope(kFormatVersion + 1, "{}"));
  LibraryRepository boot(store);
  auto r = boot.load();
  CHECK(r.status().code == Err::UnsupportedVersion);
}

TEST_CASE("repository: write failures keep the previous library intact") {
  testkit::MemoryBlobStore store;
  LibraryRepository repo(store);
  RemoteLibrary lib = makeRichLibrary();
  REQUIRE(repo.save(lib).ok());
  const std::string before = snapshot(lib);

  lib.addCategory("Nueva");
  // Only a category changed, so a save is exactly two writes (backup index, then the index = commit).
  for (int allowed : {0, 1}) {
    store.failWritesAfter(allowed);
    CHECK(repo.save(lib).code == Err::StorageFailure);
    store.failWritesAfter(-1);
    LibraryRepository boot(store);
    auto r = boot.load();
    REQUIRE(r.ok());
    CHECK(snapshot(r.value().library) == before);
  }
  REQUIRE(repo.save(lib).ok());  // and a later save works
}

// ------------------------------------------------------------------ legacy format 1 (golden fixture)

TEST_CASE("legacy: the frozen format-1 file still loads (RAW stored as number arrays)") {
  const std::string text = fixture("library_v1.irs");
  REQUIRE(text.rfind("IRIS 1 ", 0) == 0);
  auto lib = deserializeLegacyLibrary(text);
  REQUIRE(lib.ok());
  const LibraryData& d = lib.value();
  REQUIRE(d.remotes.size() == 2);
  CHECK(d.remotes[0].name == "Televisión");
  REQUIRE(d.remotes[0].buttons.size() == 4);
  const Button& power = d.remotes[0].buttons[0];
  REQUIRE(power.signal.has_value());
  CHECK(power.signal->initial[0].decoded.protocol == Protocol::Sony);
  CHECK(power.signal->initial[0].decoded.command == 18);
  const Button& raw = d.remotes[0].buttons[1];
  CHECK(raw.signal->initial[0].raw.durationsUs.size() == 10);
  CHECK(raw.signal->initial[0].raw.carrierHz == 38000);
  const Button& learnedUnknown = d.remotes[0].buttons[2];
  CHECK(learnedUnknown.signal->provenance == Provenance::Learned);
  CHECK(learnedUnknown.signal->originalCapture.has_value());
  const Button& learnedNec = d.remotes[1].buttons[0];
  CHECK(learnedNec.signal->recognition == Recognition::Recognized);
  CHECK(learnedNec.signal->initial[0].decoded.address == 4);
  CHECK(learnedNec.signal->originalCapture->durationsUs.size() == 67);
}

TEST_CASE("legacy: the repository migrates a format-1 library on the next save") {
  testkit::MemoryBlobStore store;
  store.put(LibraryRepository::legacyKey(), fixture("library_v1.irs"));
  LibraryRepository repo(store);
  auto r = repo.load();
  REQUIRE(r.ok());
  CHECK(r.value().source == LoadSource::Legacy);
  const std::string migrated = snapshot(r.value().library);

  REQUIRE(repo.save(r.value().library).ok());
  CHECK_FALSE(store.exists(LibraryRepository::legacyKey()));   // superseded
  CHECK(store.exists(LibraryRepository::indexKey()));

  LibraryRepository boot(store);
  auto again = boot.load();
  REQUIRE(again.ok());
  CHECK(again.value().source == LoadSource::Primary);
  CHECK(snapshot(again.value().library) == migrated);          // nothing lost in the migration
}

TEST_CASE("legacy: damaged legacy files are an error, not an empty library") {
  testkit::MemoryBlobStore store;
  store.put(LibraryRepository::legacyKey(), "garbage");
  LibraryRepository repo(store);
  CHECK(repo.load().status().code == Err::Corrupt);
  CHECK(store.exists(LibraryRepository::legacyKey()));
}

// ------------------------------------------------------------------ memory budget

TEST_CASE("worst-case remote file stays within the RAM budget on the ESP32") {
  // A remote that fills every limit: the maximum number of buttons, all with a learned signal, using the
  // whole per-remote RAW budget. Budget: file text + the parsed JSON document <= 96 KB (the ESP32 has
  // roughly 200-250 KB of free heap with the UI running; unverified, see docs/risks.md). Heap use is
  // measured with a counting allocator (alloc_probe.cpp); a 64-bit PC overestimates the 32-bit device.
  RemoteLibrary lib;
  Id cat = lib.addCategory("TV").value();
  Id remote = lib.addRemote("Maximo", cat).value();
  const size_t perButton = limits::kMaxRawEntriesPerRemote / limits::kMaxButtonsPerRemote / 2;  // original + raw frame
  for (size_t i = 0; i < limits::kMaxButtonsPerRemote; ++i) {
    Id b = lib.addButton(remote, "Boton " + std::to_string(i)).value();
    std::vector<uint16_t> d(perButton, 560);
    IrSignal s = buildRawSignal(d, 38000).value();
    s.provenance = Provenance::Learned;
    RawFrame original;
    original.durationsUs = d;
    s.originalCapture = original;
    REQUIRE(lib.setButtonSignal(remote, b, s).ok());
  }
  allocResetPeak();
  const size_t baseline = allocLiveBytes();
  const std::string text = serializeRemoteFile(lib.data().remotes[0]).value();
  const size_t serializePeak = allocPeakBytes() - baseline;  // document + output text while building
  CHECK(text.size() <= 48 * 1024);
  CHECK(serializePeak <= 96 * 1024);

  const std::string body = text.substr(text.find(char(10)) + 1);  // skip the header line
  allocResetPeak();
  const size_t before = allocLiveBytes();
  {
    JsonDocument doc;
    REQUIRE_FALSE(deserializeJson(doc, body.data(), body.size()));
  }
  const size_t parsePeak = allocPeakBytes() - before;  // the parsed document
  CHECK(text.size() + parsePeak <= 96 * 1024);          // text held by the caller + document
  CHECK(estimateRemoteFileBytes(lib.data().remotes[0]) >= text.size());  // the heap guard never underestimates
}

TEST_CASE("an index with the maximum number of remotes and categories is small") {
  IndexData idx;
  for (Id i = 1; i <= limits::kMaxCategories; ++i) idx.categories.push_back({i, std::string(limits::kMaxNameBytes, 'c')});
  for (Id i = 1; i <= limits::kMaxRemotes; ++i) {
    idx.remotes.push_back({100 + i, std::string(limits::kMaxNameBytes, 'r'), 1, "r999999g999999.irs", 0xFFFFFFFFu});
  }
  CHECK(serializeIndex(idx).value().size() <= 8 * 1024);
}
