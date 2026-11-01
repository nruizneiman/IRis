#include "persistence/LibraryRepository.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>

#include "persistence/Crc32.h"

namespace iris {

namespace {

bool isTransient(const Status& st) {
  return st.code != Err::Corrupt && st.code != Err::UnsupportedVersion && st.code != Err::NotFound;
}

std::string badKey(Id id) {
  char buf[24];
  std::snprintf(buf, sizeof buf, "bad%lu.irs", static_cast<unsigned long>(id));
  return buf;
}

}  // namespace

std::string LibraryRepository::remoteKey(Id id, uint32_t generation) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "r%lug%lu.irs", static_cast<unsigned long>(id), static_cast<unsigned long>(generation));
  return buf;
}

bool LibraryRepository::isRemoteFileKey(const std::string& key) {
  unsigned long id = 0, gen = 0;
  char tail[8] = {0};
  return std::sscanf(key.c_str(), "r%lug%lu%7s", &id, &gen, tail) == 3 && std::string(tail) == ".irs";
}

uint32_t LibraryRepository::highestGenerationOnDisk() const {
  uint32_t best = generation_;
  for (const std::string& key : store_.list()) {
    unsigned long id = 0, gen = 0;
    char tail[8] = {0};
    if (std::sscanf(key.c_str(), "r%lug%lu%7s", &id, &gen, tail) == 3 && std::string(tail) == ".irs") {
      best = std::max<uint32_t>(best, static_cast<uint32_t>(gen));
    }
  }
  for (const char* k : {indexKey(), backupIndexKey()}) {
    auto text = store_.read(k);
    if (!text.ok()) continue;
    auto idx = deserializeIndex(text.value());
    if (idx.ok()) best = std::max(best, idx.value().generation);
  }
  return best;
}

Result<LibraryData> LibraryRepository::assemble(const IndexData& index, std::vector<std::string>& lost) {
  LibraryData data;
  data.nextId = index.nextId;
  data.categories = index.categories;
  for (const IndexEntry& e : index.remotes) {
    Remote r;
    bool ok = false;
    Result<std::string> text = store_.read(e.fileKey);
    if (!text.ok()) {
      if (isTransient(text.status())) return text.status();  // an I/O error is not damage
    } else if (crc32(text.value()) == e.fileCrc) {
      Result<Remote> parsed = deserializeRemoteFile(text.value());
      if (parsed.ok() && parsed.value().id == e.id) {
        r = std::move(parsed.value());
        ok = true;
      } else if (!parsed.ok() && isTransient(parsed.status())) {
        return parsed.status();  // e.g. out of memory while parsing
      }
    }
    if (!ok) {
      lost.push_back(e.name);
      if (text.ok()) store_.write(badKey(e.id), text.value());  // keep the bytes for later inspection
      continue;
    }
    r.id = e.id;
    r.name = e.name;
    r.categoryId = e.categoryId;
    data.remotes.push_back(std::move(r));
  }
  return data;
}

Result<LoadOutcome> LibraryRepository::load() {
  committed_.clear();
  generation_ = 0;

  const bool hasIndex = store_.exists(indexKey());
  const bool hasBackup = store_.exists(backupIndexKey());
  if (!hasIndex && !hasBackup) {
    // Maybe a format-1 library from an older firmware.
    for (const char* key : {legacyKey(), legacyBackupKey()}) {
      if (!store_.exists(key)) continue;
      Result<std::string> text = store_.read(key);
      if (!text.ok()) return text.status();
      Result<LibraryData> legacy = deserializeLegacyLibrary(text.value());
      if (!legacy.ok()) {
        if (isTransient(legacy.status())) return legacy.status();
        continue;
      }
      Result<RemoteLibrary> lib = RemoteLibrary::fromData(std::move(legacy.value()));
      if (!lib.ok()) continue;
      return LoadOutcome{std::move(lib.value()), LoadSource::Legacy, Status::success(), {}};
    }
    if (store_.exists(legacyKey()) || store_.exists(legacyBackupKey())) {
      return Status::fail(Err::Corrupt, "legacy library files are unreadable");
    }
    return LoadOutcome{};
  }

  Status primaryStatus = Status::fail(Err::NotFound, "primary index missing");
  const char* order[2] = {indexKey(), backupIndexKey()};
  for (int pass = 0; pass < 2; ++pass) {
    const char* key = order[pass];
    if (!store_.exists(key)) continue;
    Result<std::string> text = store_.read(key);
    if (!text.ok()) return text.status();  // transient: do not fall back and do not touch anything
    Result<IndexData> index = deserializeIndex(text.value());
    Status st = index.status();
    std::vector<std::string> lost;
    if (index.ok()) {
      Result<LibraryData> data = assemble(index.value(), lost);
      if (!data.ok()) {
        if (isTransient(data.status())) return data.status();
        st = data.status();
      } else {
        Result<RemoteLibrary> lib = RemoteLibrary::fromData(std::move(data.value()));
        if (lib.ok()) {
          generation_ = index.value().generation;
          for (const IndexEntry& e : index.value().remotes) {
            if (std::find(lost.begin(), lost.end(), e.name) == lost.end()) {
              committed_[e.id] = Committed{e.id, e.fileKey, e.fileCrc};
            }
          }
          return LoadOutcome{std::move(lib.value()), pass == 0 ? LoadSource::Primary : LoadSource::Backup, primaryStatus,
                             std::move(lost)};
        }
        st = lib.status();
      }
    } else if (isTransient(st) || st.code == Err::UnsupportedVersion) {
      // Transient trouble, or data from a NEWER firmware: do not fall back to an older backup (the next save
      // would overwrite the newer data). The caller must keep the files and stop saving.
      return st;
    }
    if (pass == 0) primaryStatus = st;
  }
  return Status::fail(Err::Corrupt, "no readable library index: " + primaryStatus.message);
}

Status LibraryRepository::save(const RemoteLibrary& library) {
  const LibraryData& data = library.data();

  // Learn what is on disk if load() was not called (or failed), so generations never collide.
  const uint32_t gen = highestGenerationOnDisk() + 1;

  IndexData index;
  index.nextId = data.nextId;
  index.generation = gen;
  index.categories = data.categories;
  std::map<Id, Committed> next;
  for (const Remote& r : data.remotes) {
    Result<std::string> text = serializeRemoteFile(r);
    if (!text.ok()) return text.status();  // e.g. not enough memory: nothing was written
    const uint32_t crc = crc32(text.value());
    Committed c;
    c.remoteId = r.id;
    auto it = committed_.find(r.id);
    if (it != committed_.end() && it->second.crc == crc && store_.exists(it->second.key)) {
      c = it->second;  // unchanged: keep the existing file
    } else {
      c.key = remoteKey(r.id, gen);
      c.crc = crc;
      Status st = store_.write(c.key, text.value());
      if (!st.ok()) return Status::fail(Err::StorageFailure, "remote file write failed: " + st.message);
    }
    next[r.id] = c;
    index.remotes.push_back(IndexEntry{r.id, r.name, r.categoryId, c.key, c.crc});
  }

  Result<std::string> indexText = serializeIndex(index);
  if (!indexText.ok()) return indexText.status();

  // Rotate the backup only from an index that is still intact, so damage is never promoted.
  if (store_.exists(indexKey())) {
    Result<std::string> current = store_.read(indexKey());
    if (current.ok() && checkEnvelope(current.value()).ok()) {
      Status st = store_.write(backupIndexKey(), current.value());
      if (!st.ok()) return Status::fail(Err::StorageFailure, "backup index write failed: " + st.message);
    }
  }
  Status st = store_.write(indexKey(), indexText.value());  // COMMIT POINT
  if (!st.ok()) return Status::fail(Err::StorageFailure, "index write failed: " + st.message);

  committed_ = std::move(next);
  generation_ = gen;
  // A format-1 library is superseded once the new layout is committed.
  for (const char* key : {legacyKey(), legacyBackupKey()}) {
    if (store_.exists(key)) store_.remove(key);
  }
  collectGarbage();
  return Status::success();
}

void LibraryRepository::collectGarbage() {
  std::set<std::string> keep;
  for (const char* key : {indexKey(), backupIndexKey()}) {
    auto text = store_.read(key);
    if (!text.ok()) continue;
    auto idx = deserializeIndex(text.value());
    if (!idx.ok()) continue;
    for (const IndexEntry& e : idx.value().remotes) keep.insert(e.fileKey);
  }
  for (const std::string& key : store_.list()) {
    if (isRemoteFileKey(key) && keep.count(key) == 0) store_.remove(key);  // best effort
  }
}

}  // namespace iris
