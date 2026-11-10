// The signal sequences behind the loop, crazy and Be Gone modes (see SequenceRunner). Pure logic: the card and the
// library are reached through IFileSource and RemoteLibrary, so everything here runs on the PC in tests.
#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "app/RemoteLibrary.h"
#include "app/RemoteView.h"
#include "app/SequenceRunner.h"
#include "hardware/IFileSource.h"

namespace iris {

// Loop: the chosen buttons in the chosen order, again and again (`rounds` 0 = until stopped). Buttons without a signal
// are skipped. The remote is copied, so the sequence does not depend on the library or the card afterwards.
class LoopSequence : public ISignalSequence {
 public:
  LoopSequence(RemoteView view, std::vector<size_t> order, uint32_t rounds = 0);
  bool next(SequenceItem& out) override;
  std::string progress() const override;

 private:
  RemoteView view_;
  std::vector<size_t> order_;
  uint32_t rounds_, round_ = 0;
  size_t pos_ = 0;
};

// Crazy: random buttons of one remote, sometimes the same one two or three times in a row, with a random pause.
// `seed` makes a run repeatable in tests; the screen seeds it from the clock.
class CrazySequence : public ISignalSequence {
 public:
  CrazySequence(RemoteView view, uint32_t seed);
  bool next(SequenceItem& out) override;
  std::string progress() const override { return {}; }
  uint32_t gapAfterMs(uint32_t defaultGapMs) override;

 private:
  uint32_t rand();
  RemoteView view_;
  std::vector<size_t> usable_;
  uint32_t state_;
  uint32_t repeatsLeft_ = 0;
  size_t current_ = 0;
};

// Where Be Gone gets its controls from, one at a time.
class IRemoteViewSource {
 public:
  virtual ~IRemoteViewSource() = default;
  virtual bool nextView(RemoteView& out) = 0;  // false = no more
  virtual std::string progress() const { return {}; }
};

// All the saved controls of one category.
class SavedCategorySource : public IRemoteViewSource {
 public:
  SavedCategorySource(const RemoteLibrary& library, Id categoryId);
  bool nextView(RemoteView& out) override;
  std::string progress() const override;

 private:
  const RemoteLibrary& library_;
  std::vector<Id> remotes_;
  size_t next_ = 0;
};

// Every `.ir` file under a card folder, subfolders included, in name order. Files that cannot be read are skipped and
// counted. Memory stays small: one folder listing and one file at a time.
class CardFolderSource : public IRemoteViewSource {
 public:
  static constexpr size_t kMaxEntriesPerFolder = 400;
  CardFolderSource(IFileSource& files, std::string root);
  bool nextView(RemoteView& out) override;
  std::string progress() const override;
  size_t filesRead() const { return read_; }
  size_t filesSkipped() const { return skipped_; }

 private:
  IFileSource& files_;
  std::vector<std::string> dirs_;  // folders still to open (last = next)
  std::deque<std::string> queue_;  // files of the current folder
  size_t read_ = 0, skipped_ = 0;
};

// Be Gone: for each control from the source, its power button (off or on, see findPowerButton). A code already sent in
// this run is not sent again, which is what keeps a run over a whole IRDB folder short.
class BeGoneSequence : public ISignalSequence {
 public:
  static constexpr size_t kMaxRemembered = 4000;
  BeGoneSequence(std::unique_ptr<IRemoteViewSource> source, PowerWanted wanted);
  bool next(SequenceItem& out) override;
  std::string progress() const override;
  size_t duplicatesSkipped() const { return duplicates_; }

 private:
  bool remember(const IrSignal& s);  // true when it was new
  std::unique_ptr<IRemoteViewSource> source_;
  PowerWanted wanted_;
  std::vector<uint64_t> seen_;  // sorted
  size_t duplicates_ = 0, controls_ = 0;
};

// A 64-bit fingerprint of what a signal puts on the air (its first frame), for de-duplication.
uint64_t signalFingerprint(const IrSignal& s);

}  // namespace iris
