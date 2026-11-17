// Finds which controls hold the codes the device just heard: the saved library first, then every `.ir` file under
// /irdb on the card, one file per step so the screen stays alive and the search can be stopped.
//
// Only decoded signals are compared (same protocol, address, command and size; the RC5 toggle bit is ignored because it
// changes on every key press). A RAW capture has no code to look up, and a card button that was only kept as a raw frame
// cannot be matched to a decoded one, so both are left out. Pure logic: the card is reached through IFileSource.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "app/RemoteLibrary.h"
#include "app/Sequences.h"
#include "domain/IrSignal.h"
#include "hardware/IFileSource.h"

namespace iris {

// True when the first frame is a decoded one, so there is a code to compare.
bool isMatchable(const IrSignal& s);
// Both are matchable and carry the same code.
bool sameCode(const IrSignal& a, const IrSignal& b);

struct MatchedButton {
  std::string name;
  size_t position = 0;  // index of the button in its control
  size_t heard = 0;     // which of the heard codes it matched (index into RemoteMatcher::heard())
};

struct MatchHit {
  std::string remote;
  bool fromCard = false;
  Id savedId = 0;       // library controls
  std::string path;     // card controls
  std::vector<MatchedButton> buttons;
  size_t heardMatched = 0;  // how many different heard codes this control has
};

class RemoteMatcher {
 public:
  static constexpr size_t kMaxHeard = 16;
  static constexpr size_t kMaxHits = 60;
  static constexpr size_t kMaxButtonsPerHit = 12;

  // `card` may be null. The saved controls are searched right away; the card is searched by step().
  // Signals that cannot be matched, and repeated codes, are dropped from `heard`.
  RemoteMatcher(const RemoteLibrary& library, IFileSource* card, const std::vector<IrSignal>& heard);

  // Reads one more card file. False when there is nothing left to read (or the hit list is full).
  bool step();
  bool done() const { return done_; }
  void stop() { done_ = true; }

  const std::vector<IrSignal>& heard() const { return heard_; }
  const std::vector<MatchHit>& hits() const { return hits_; }
  size_t filesRead() const { return source_ ? source_->filesRead() : 0; }
  bool full() const { return full_; }  // more controls matched than kMaxHits

  // Controls with more of the heard codes first; the order of discovery within the same count.
  void sortByScore();

 private:
  void consider(const std::string& name, bool fromCard, Id savedId, const std::string& path,
                const std::vector<std::pair<std::string, const std::optional<IrSignal>*>>& buttons);

  std::vector<IrSignal> heard_;
  std::vector<MatchHit> hits_;
  std::unique_ptr<CardFolderSource> source_;
  bool done_ = false, full_ = false;
};

}  // namespace iris
