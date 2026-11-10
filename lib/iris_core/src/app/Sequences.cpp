#include "app/Sequences.h"

#include <algorithm>
#include <cctype>

namespace iris {

namespace {

constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

void mix(uint64_t& h, uint64_t v) {
  for (int i = 0; i < 8; ++i) {
    h ^= (v >> (8 * i)) & 0xFF;
    h *= kFnvPrime;
  }
}

bool usable(const ViewButton& b) { return b.signal.has_value(); }

}  // namespace

uint64_t signalFingerprint(const IrSignal& s) {
  uint64_t h = kFnvOffset;
  if (s.initial.empty()) return h;
  const Frame& f = s.initial[0];
  mix(h, static_cast<uint64_t>(f.kind));
  if (f.kind == Frame::Kind::Decoded) {
    mix(h, static_cast<uint64_t>(f.decoded.protocol));
    mix(h, f.decoded.address);
    mix(h, f.decoded.command);
    mix(h, f.decoded.bits);
    mix(h, f.decoded.toggle ? 1 : 0);
    for (uint8_t b : f.decoded.state) mix(h, b);
  } else {
    mix(h, f.raw.carrierHz);
    for (uint16_t d : f.raw.durationsUs) mix(h, d);
  }
  return h;
}

// ---------------------------------------------------------------- loop

LoopSequence::LoopSequence(RemoteView view, std::vector<size_t> order, uint32_t rounds)
    : view_(std::move(view)), rounds_(rounds) {
  for (size_t i : order) {
    if (i < view_.buttons.size() && usable(view_.buttons[i])) order_.push_back(i);
  }
}

bool LoopSequence::next(SequenceItem& out) {
  if (order_.empty()) return false;
  if (pos_ >= order_.size()) {
    pos_ = 0;
    ++round_;
  }
  if (rounds_ != 0 && round_ >= rounds_) return false;
  const ViewButton& b = view_.buttons[order_[pos_++]];
  out.label = b.name;
  out.signal = *b.signal;
  return true;
}

std::string LoopSequence::progress() const {
  return "#" + std::to_string(round_ + 1) + (rounds_ ? "/" + std::to_string(rounds_) : std::string());
}

// ---------------------------------------------------------------- crazy

CrazySequence::CrazySequence(RemoteView view, uint32_t seed) : view_(std::move(view)), state_(seed ? seed : 0x9E3779B9u) {
  for (size_t i = 0; i < view_.buttons.size(); ++i) {
    if (usable(view_.buttons[i])) usable_.push_back(i);
  }
}

uint32_t CrazySequence::rand() {  // xorshift32: small, fast, plenty for picking buttons
  state_ ^= state_ << 13;
  state_ ^= state_ >> 17;
  state_ ^= state_ << 5;
  return state_;
}

bool CrazySequence::next(SequenceItem& out) {
  if (usable_.empty()) return false;
  if (repeatsLeft_ > 0) {
    --repeatsLeft_;
  } else {
    current_ = usable_[rand() % usable_.size()];
    const uint32_t r = rand() % 10;
    repeatsLeft_ = r < 6 ? 0 : (r < 9 ? 1 : 2);  // mostly once, sometimes twice, rarely three times in a row
  }
  const ViewButton& b = view_.buttons[current_];
  out.label = b.name;
  out.signal = *b.signal;
  return true;
}

uint32_t CrazySequence::gapAfterMs(uint32_t defaultGapMs) {
  if (repeatsLeft_ > 0) return SequenceRunner::kMinGapMs * 2;  // the same button again: quick
  return defaultGapMs / 2 + rand() % (defaultGapMs + 1);       // between 0.5x and 1.5x the chosen pause
}

// ---------------------------------------------------------------- Be Gone sources

SavedCategorySource::SavedCategorySource(const RemoteLibrary& library, Id categoryId) : library_(library) {
  for (const Remote& r : library.data().remotes) {
    if (r.categoryId == categoryId) remotes_.push_back(r.id);
  }
}

bool SavedCategorySource::nextView(RemoteView& out) {
  while (next_ < remotes_.size()) {
    Result<RemoteView> v = viewSavedRemote(library_, remotes_[next_++]);
    if (!v.ok()) continue;
    out = std::move(v.value());
    return true;
  }
  return false;
}

std::string SavedCategorySource::progress() const {
  return std::to_string(next_) + "/" + std::to_string(remotes_.size());
}

namespace {

std::string lower(std::string s) {
  for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return s;
}

}  // namespace

CardFolderSource::CardFolderSource(IFileSource& files, std::string root) : files_(files) { dirs_.push_back(std::move(root)); }

bool CardFolderSource::nextView(RemoteView& out) {
  for (;;) {
    while (!queue_.empty()) {
      const std::string path = queue_.front();
      queue_.pop_front();
      Result<RemoteView> v = viewCardRemote(files_, path);
      if (!v.ok()) {
        ++skipped_;
        continue;
      }
      ++read_;
      out = std::move(v.value());
      return true;
    }
    if (dirs_.empty()) return false;
    const std::string dir = dirs_.back();
    dirs_.pop_back();
    Result<std::vector<FileEntry>> entries = files_.list(dir, kMaxEntriesPerFolder);
    if (!entries.ok()) {
      ++skipped_;
      continue;
    }
    std::vector<FileEntry> list = std::move(entries.value());
    std::sort(list.begin(), list.end(), [](const FileEntry& a, const FileEntry& b) { return lower(a.name) < lower(b.name); });
    std::vector<std::string> subdirs;
    for (const FileEntry& e : list) {
      if (e.name.empty() || e.name[0] == '.') continue;
      if (e.isDir) subdirs.push_back(dir + "/" + e.name);
      else if (endsWithIr(e.name)) queue_.push_back(dir + "/" + e.name);
    }
    for (auto it = subdirs.rbegin(); it != subdirs.rend(); ++it) dirs_.push_back(*it);  // so the first one is opened first
  }
}

std::string CardFolderSource::progress() const { return std::to_string(read_) + " files"; }

// ---------------------------------------------------------------- Be Gone

BeGoneSequence::BeGoneSequence(std::unique_ptr<IRemoteViewSource> source, PowerWanted wanted)
    : source_(std::move(source)), wanted_(wanted) {}

bool BeGoneSequence::remember(const IrSignal& s) {
  const uint64_t fp = signalFingerprint(s);
  auto it = std::lower_bound(seen_.begin(), seen_.end(), fp);
  if (it != seen_.end() && *it == fp) return false;
  if (seen_.size() < kMaxRemembered) seen_.insert(it, fp);
  return true;
}

bool BeGoneSequence::next(SequenceItem& out) {
  RemoteView view;
  while (source_ && source_->nextView(view)) {
    ++controls_;
    const ViewButton* power = findPowerButton(view, wanted_);
    if (!power) continue;
    if (!remember(*power->signal)) {
      ++duplicates_;
      continue;
    }
    out.label = view.name;
    out.signal = *power->signal;
    return true;
  }
  return false;
}

std::string BeGoneSequence::progress() const {
  return source_ ? source_->progress() : std::string();
}

}  // namespace iris
