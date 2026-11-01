#include "app/RemoteLibrary.h"

#include <algorithm>
#include <limits>

#include "domain/Limits.h"
#include "domain/Naming.h"
#include "domain/SignalValidation.h"

namespace iris {

namespace {

template <class Range>
bool nameTaken(const Range& items, std::string_view name, Id ignoreId) {
  for (const auto& item : items) {
    if (item.id != ignoreId && namesEqual(item.name, name)) return true;
  }
  return false;
}

Status checkName(const std::string& raw, std::string& out) {
  out = trimName(raw);
  return validateName(out);
}

}  // namespace

Status validateLibraryData(const LibraryData& d) {
  if (d.nextId == 0) return Status::fail(Err::Corrupt, "nextId is zero");
  if (d.categories.size() > limits::kMaxCategories) return Status::fail(Err::LimitReached, "too many categories");
  if (d.remotes.size() > limits::kMaxRemotes) return Status::fail(Err::LimitReached, "too many remotes");

  std::vector<Id> seen;
  auto claim = [&](Id id) {
    if (id == 0 || id >= d.nextId) return false;
    if (std::find(seen.begin(), seen.end(), id) != seen.end()) return false;
    seen.push_back(id);
    return true;
  };

  for (const Category& c : d.categories) {
    if (!claim(c.id)) return Status::fail(Err::Corrupt, "bad or duplicate category id");
    Status st = validateName(c.name);
    if (!st.ok()) return Status::fail(Err::Corrupt, "category: " + st.message);
    if (nameTaken(d.categories, c.name, c.id)) return Status::fail(Err::Corrupt, "duplicate category name");
  }

  size_t totalRaw = 0;
  for (const Remote& r : d.remotes) {
    if (!claim(r.id)) return Status::fail(Err::Corrupt, "bad or duplicate remote id");
    Status st = validateName(r.name);
    if (!st.ok()) return Status::fail(Err::Corrupt, "remote: " + st.message);
    if (nameTaken(d.remotes, r.name, r.id)) return Status::fail(Err::Corrupt, "duplicate remote name");
    const bool categoryExists = std::any_of(d.categories.begin(), d.categories.end(),
                                            [&](const Category& c) { return c.id == r.categoryId; });
    if (!categoryExists) return Status::fail(Err::Corrupt, "remote references missing category");
    if (r.buttons.size() > limits::kMaxButtonsPerRemote) return Status::fail(Err::LimitReached, "too many buttons");
    size_t remoteRaw = 0;
    for (const Button& b : r.buttons) {
      if (!claim(b.id)) return Status::fail(Err::Corrupt, "bad or duplicate button id");
      Status bs = validateName(b.name);
      if (!bs.ok()) return Status::fail(Err::Corrupt, "button: " + bs.message);
      if (nameTaken(r.buttons, b.name, b.id)) return Status::fail(Err::Corrupt, "duplicate button name");
      if (b.signal) {
        Status ss = validateSignal(*b.signal);
        if (!ss.ok()) return Status::fail(Err::Corrupt, "signal: " + ss.message);
        totalRaw += rawEntryCount(*b.signal);
        remoteRaw += rawEntryCount(*b.signal);
      }
    }
    if (remoteRaw > limits::kMaxRawEntriesPerRemote) return Status::fail(Err::LimitReached, "remote raw data too large");
  }
  if (totalRaw > limits::kMaxTotalRawEntries) return Status::fail(Err::LimitReached, "library raw data too large");
  return Status::success();
}

Result<RemoteLibrary> RemoteLibrary::fromData(LibraryData data) {
  Status st = validateLibraryData(data);
  if (!st.ok()) return st;
  RemoteLibrary lib;
  lib.data_ = std::move(data);
  return lib;
}

Result<Id> RemoteLibrary::allocateId() {
  if (data_.nextId == std::numeric_limits<Id>::max()) {
    return Status::fail(Err::LimitReached, "id space exhausted");
  }
  return data_.nextId++;
}

const Category* RemoteLibrary::findCategory(Id id) const {
  for (const auto& c : data_.categories) if (c.id == id) return &c;
  return nullptr;
}
const Remote* RemoteLibrary::findRemote(Id id) const {
  for (const auto& r : data_.remotes) if (r.id == id) return &r;
  return nullptr;
}
const Button* RemoteLibrary::findButton(Id remoteId, Id buttonId) const {
  const Remote* r = findRemote(remoteId);
  if (!r) return nullptr;
  for (const auto& b : r->buttons) if (b.id == buttonId) return &b;
  return nullptr;
}
Category* RemoteLibrary::findCategoryMut(Id id) { return const_cast<Category*>(findCategory(id)); }
Remote* RemoteLibrary::findRemoteMut(Id id) { return const_cast<Remote*>(findRemote(id)); }
Button* RemoteLibrary::findButtonMut(Id r, Id b) { return const_cast<Button*>(findButton(r, b)); }

size_t RemoteLibrary::totalRawEntries() const {
  size_t n = 0;
  for (const auto& r : data_.remotes)
    for (const auto& b : r.buttons)
      if (b.signal) n += rawEntryCount(*b.signal);
  return n;
}

// ---- categories ----

Result<Id> RemoteLibrary::addCategory(const std::string& rawName) {
  std::string name;
  Status st = checkName(rawName, name);
  if (!st.ok()) return st;
  if (data_.categories.size() >= limits::kMaxCategories) return Status::fail(Err::LimitReached, "category limit reached");
  if (nameTaken(data_.categories, name, 0)) return Status::fail(Err::AlreadyExists, "category name already used");
  Result<Id> id = allocateId();
  if (!id.ok()) return id;
  data_.categories.push_back(Category{id.value(), name});
  dirty_ = true;
  return id;
}

Status RemoteLibrary::renameCategory(Id id, const std::string& rawName) {
  Category* c = findCategoryMut(id);
  if (!c) return Status::fail(Err::NotFound, "category not found");
  std::string name;
  Status st = checkName(rawName, name);
  if (!st.ok()) return st;
  if (nameTaken(data_.categories, name, id)) return Status::fail(Err::AlreadyExists, "category name already used");
  c->name = name;
  dirty_ = true;
  return Status::success();
}

Status RemoteLibrary::removeCategory(Id id) {
  if (!findCategory(id)) return Status::fail(Err::NotFound, "category not found");
  for (const auto& r : data_.remotes) {
    if (r.categoryId == id) return Status::fail(Err::InUse, "category still has remotes");
  }
  data_.categories.erase(std::remove_if(data_.categories.begin(), data_.categories.end(),
                                        [&](const Category& c) { return c.id == id; }),
                         data_.categories.end());
  dirty_ = true;
  return Status::success();
}

// ---- remotes ----

Result<Id> RemoteLibrary::addRemote(const std::string& rawName, Id categoryId) {
  std::string name;
  Status st = checkName(rawName, name);
  if (!st.ok()) return st;
  if (!findCategory(categoryId)) return Status::fail(Err::NotFound, "category not found");
  if (data_.remotes.size() >= limits::kMaxRemotes) return Status::fail(Err::LimitReached, "remote limit reached");
  if (nameTaken(data_.remotes, name, 0)) return Status::fail(Err::AlreadyExists, "remote name already used");
  Result<Id> id = allocateId();
  if (!id.ok()) return id;
  Remote r;
  r.id = id.value();
  r.name = name;
  r.categoryId = categoryId;
  data_.remotes.push_back(std::move(r));
  dirty_ = true;
  return id;
}

Result<Id> RemoteLibrary::importRemote(const std::string& name, Id categoryId,
                                       const std::vector<std::pair<std::string, IrSignal>>& buttons) {
  const bool wasDirty = dirty_;
  Result<Id> remote = addRemote(name, categoryId);
  if (!remote.ok()) return remote;
  for (const auto& b : buttons) {
    Result<Id> id = addButton(remote.value(), b.first);
    Status st = id.ok() ? setButtonSignal(remote.value(), id.value(), b.second) : id.status();
    if (!st.ok()) {
      removeRemote(remote.value());
      dirty_ = wasDirty;  // nothing visible changed (an id was consumed, which is harmless)
      return st;
    }
  }
  return remote;
}

Status RemoteLibrary::renameRemote(Id id, const std::string& rawName) {
  Remote* r = findRemoteMut(id);
  if (!r) return Status::fail(Err::NotFound, "remote not found");
  std::string name;
  Status st = checkName(rawName, name);
  if (!st.ok()) return st;
  if (nameTaken(data_.remotes, name, id)) return Status::fail(Err::AlreadyExists, "remote name already used");
  r->name = name;
  dirty_ = true;
  return Status::success();
}

Status RemoteLibrary::moveRemote(Id id, Id newCategoryId) {
  Remote* r = findRemoteMut(id);
  if (!r) return Status::fail(Err::NotFound, "remote not found");
  if (!findCategory(newCategoryId)) return Status::fail(Err::NotFound, "category not found");
  r->categoryId = newCategoryId;
  dirty_ = true;
  return Status::success();
}

Status RemoteLibrary::removeRemote(Id id) {
  if (!findRemote(id)) return Status::fail(Err::NotFound, "remote not found");
  data_.remotes.erase(std::remove_if(data_.remotes.begin(), data_.remotes.end(),
                                     [&](const Remote& r) { return r.id == id; }),
                      data_.remotes.end());
  dirty_ = true;
  return Status::success();
}

Result<Id> RemoteLibrary::duplicateRemote(Id sourceId, const std::string& rawName) {
  const Remote* src = findRemote(sourceId);
  if (!src) return Status::fail(Err::NotFound, "remote not found");
  std::string name;
  Status st = checkName(rawName, name);
  if (!st.ok()) return st;
  if (data_.remotes.size() >= limits::kMaxRemotes) return Status::fail(Err::LimitReached, "remote limit reached");
  if (nameTaken(data_.remotes, name, 0)) return Status::fail(Err::AlreadyExists, "remote name already used");
  size_t srcRaw = 0;
  for (const Button& b : src->buttons) srcRaw += b.signal ? rawEntryCount(*b.signal) : 0;
  if (totalRawEntries() + srcRaw > limits::kMaxTotalRawEntries) {
    return Status::fail(Err::LimitReached, "raw storage budget exceeded");
  }
  if (data_.nextId > std::numeric_limits<Id>::max() - static_cast<Id>(src->buttons.size()) - 2) {
    return Status::fail(Err::LimitReached, "id space exhausted");
  }
  Remote copy = *src;  // copy first: allocating ids below must not invalidate `src`
  copy.id = data_.nextId++;
  copy.name = name;
  for (Button& b : copy.buttons) b.id = data_.nextId++;
  const Id newId = copy.id;
  data_.remotes.push_back(std::move(copy));
  dirty_ = true;
  return newId;
}

// ---- buttons ----

Result<Id> RemoteLibrary::addButton(Id remoteId, const std::string& rawName) {
  Remote* r = findRemoteMut(remoteId);
  if (!r) return Status::fail(Err::NotFound, "remote not found");
  std::string name;
  Status st = checkName(rawName, name);
  if (!st.ok()) return st;
  if (r->buttons.size() >= limits::kMaxButtonsPerRemote) return Status::fail(Err::LimitReached, "button limit reached");
  if (nameTaken(r->buttons, name, 0)) return Status::fail(Err::AlreadyExists, "button name already used");
  Result<Id> id = allocateId();
  if (!id.ok()) return id;
  Button b;
  b.id = id.value();
  b.name = name;
  r->buttons.push_back(std::move(b));
  dirty_ = true;
  return id;
}

Status RemoteLibrary::renameButton(Id remoteId, Id buttonId, const std::string& rawName) {
  Remote* r = findRemoteMut(remoteId);
  Button* b = findButtonMut(remoteId, buttonId);
  if (!r || !b) return Status::fail(Err::NotFound, "button not found");
  std::string name;
  Status st = checkName(rawName, name);
  if (!st.ok()) return st;
  if (nameTaken(r->buttons, name, buttonId)) return Status::fail(Err::AlreadyExists, "button name already used");
  b->name = name;
  dirty_ = true;
  return Status::success();
}

Status RemoteLibrary::removeButton(Id remoteId, Id buttonId) {
  Remote* r = findRemoteMut(remoteId);
  if (!r || !findButton(remoteId, buttonId)) return Status::fail(Err::NotFound, "button not found");
  r->buttons.erase(std::remove_if(r->buttons.begin(), r->buttons.end(),
                                  [&](const Button& b) { return b.id == buttonId; }),
                   r->buttons.end());
  dirty_ = true;
  return Status::success();
}

Status RemoteLibrary::setButtonSignal(Id remoteId, Id buttonId, const IrSignal& signal) {
  Button* b = findButtonMut(remoteId, buttonId);
  if (!b) return Status::fail(Err::NotFound, "button not found");
  Status st = validateSignal(signal);
  if (!st.ok()) return st;
  const size_t current = b->signal ? rawEntryCount(*b->signal) : 0;
  const size_t projected = totalRawEntries() - current + rawEntryCount(signal);
  if (projected > limits::kMaxTotalRawEntries) return Status::fail(Err::LimitReached, "raw storage budget exceeded");
  const Remote* owner = findRemote(remoteId);
  size_t remoteRaw = 0;
  for (const Button& other : owner->buttons) {
    if (other.signal && other.id != buttonId) remoteRaw += rawEntryCount(*other.signal);
  }
  if (remoteRaw + rawEntryCount(signal) > limits::kMaxRawEntriesPerRemote) {
    return Status::fail(Err::LimitReached, "raw storage budget of this remote exceeded");
  }
  b->signal = signal;
  dirty_ = true;
  return Status::success();
}

Status RemoteLibrary::clearButtonSignal(Id remoteId, Id buttonId) {
  Button* b = findButtonMut(remoteId, buttonId);
  if (!b) return Status::fail(Err::NotFound, "button not found");
  b->signal.reset();
  dirty_ = true;
  return Status::success();
}

}  // namespace iris
