// Loads and saves the library through IBlobStore with an all-or-nothing commit.
//
// Layout: "idx.irs" (index: categories, remotes, and for each remote the key + CRC of its file),
// "idx.prev" (the previous index = one generation of backup) and one "r<id>g<gen>.irs" per remote.
// Memory use is proportional to ONE remote, not to the whole library.
//
// save() = write the changed remote files under a NEW generation name, then replace the index. The
// index replacement is the commit point; it is atomic (IBlobStore contract). A cut at any moment leaves
// either the old library or the new one (test_power_cut.cpp tries every write position). Files not
// referenced by the index or the backup index are orphans and are deleted after a successful commit.
//
// load() never mistakes a transient failure (read error, out of memory) for corruption: those return the
// original error and nothing is touched, so the caller can keep the files and refuse to overwrite them.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "Result.h"
#include "app/RemoteLibrary.h"
#include "hardware/IBlobStore.h"
#include "persistence/LibrarySerializer.h"

namespace iris {

enum class LoadSource { Empty, Primary, Backup, Legacy };

struct LoadOutcome {
  RemoteLibrary library;
  LoadSource source = LoadSource::Empty;
  Status primaryStatus;                    // why the primary index was not used (ok for Primary / Empty)
  std::vector<std::string> lostRemotes;    // names of remotes whose file was unreadable and were left out
};

class LibraryRepository {
 public:
  explicit LibraryRepository(IBlobStore& store) : store_(store) {}

  // Nothing stored -> empty library. Primary index unusable (corrupt) -> backup index. Both unusable -> Err::Corrupt.
  // A remote file that fails its checks is left out and reported in lostRemotes (its bytes are kept under a
  // "bad<id>.irs" key) instead of discarding the whole library. Transient errors are returned as they are.
  Result<LoadOutcome> load();

  // Err::StorageFailure when the commit could not be completed (the previous library stays intact).
  Status save(const RemoteLibrary& library);

  // Generation of the last committed index this instance knows (0 = none).
  uint32_t generation() const { return generation_; }

  static const char* indexKey() { return "idx.irs"; }
  static const char* backupIndexKey() { return "idx.prev"; }
  static const char* legacyKey() { return "library.irs"; }
  static const char* legacyBackupKey() { return "library.prev"; }

 private:
  struct Committed {
    Id remoteId = 0;
    std::string key;
    uint32_t crc = 0;
  };

  Result<LibraryData> assemble(const IndexData& index, std::vector<std::string>& lost);
  void collectGarbage();
  uint32_t highestGenerationOnDisk() const;
  static std::string remoteKey(Id id, uint32_t generation);
  static bool isRemoteFileKey(const std::string& key);

  IBlobStore& store_;
  uint32_t generation_ = 0;
  std::map<Id, Committed> committed_;  // remote files referenced by the index we loaded / saved last
};

}  // namespace iris
