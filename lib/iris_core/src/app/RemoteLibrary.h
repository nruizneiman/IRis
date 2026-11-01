// Business rules for categories, remotes and buttons. Pure logic: no storage, no hardware.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Result.h"
#include "domain/Library.h"

namespace iris {

// Checks every invariant of a library (ids, names, limits, signals). Used on load and in tests.
Status validateLibraryData(const LibraryData& data);

class RemoteLibrary {
 public:
  RemoteLibrary() = default;
  // Validates `data`; refuses inconsistent or over-limit content.
  static Result<RemoteLibrary> fromData(LibraryData data);

  const LibraryData& data() const { return data_; }

  Result<Id> addCategory(const std::string& name);
  Status renameCategory(Id id, const std::string& name);
  Status removeCategory(Id id);  // InUse while any remote belongs to it

  Result<Id> addRemote(const std::string& name, Id categoryId);
  Status renameRemote(Id id, const std::string& name);
  Status moveRemote(Id id, Id newCategoryId);
  Status removeRemote(Id id);
  // Copies a remote with all its buttons and signals under a new name (same category). Counts against every limit.
  Result<Id> duplicateRemote(Id sourceId, const std::string& newName);

  // Creates a remote with all its buttons and signals, or nothing: a failure part-way removes what was added.
  Result<Id> importRemote(const std::string& name, Id categoryId,
                          const std::vector<std::pair<std::string, IrSignal>>& buttons);

  Result<Id> addButton(Id remoteId, const std::string& name);
  Status renameButton(Id remoteId, Id buttonId, const std::string& name);
  Status removeButton(Id remoteId, Id buttonId);
  Status setButtonSignal(Id remoteId, Id buttonId, const IrSignal& signal);
  Status clearButtonSignal(Id remoteId, Id buttonId);

  const Category* findCategory(Id id) const;
  const Remote* findRemote(Id id) const;
  const Button* findButton(Id remoteId, Id buttonId) const;
  size_t totalRawEntries() const;

  // dirty() is true after any successful mutation until markClean() (called after a save).
  bool dirty() const { return dirty_; }
  void markClean() { dirty_ = false; }

 private:
  Category* findCategoryMut(Id id);
  Remote* findRemoteMut(Id id);
  Button* findButtonMut(Id remoteId, Id buttonId);
  Result<Id> allocateId();

  LibraryData data_;
  bool dirty_ = false;
};

}  // namespace iris
