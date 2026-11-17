#include "app/SignalMatch.h"

#include <algorithm>

namespace iris {

bool isMatchable(const IrSignal& s) {
  return !s.initial.empty() && s.initial[0].kind == Frame::Kind::Decoded && s.initial[0].decoded.protocol != Protocol::Unknown;
}

bool sameCode(const IrSignal& a, const IrSignal& b) {
  if (!isMatchable(a) || !isMatchable(b)) return false;
  const DecodedFrame& x = a.initial[0].decoded;
  const DecodedFrame& y = b.initial[0].decoded;
  return x.protocol == y.protocol && x.address == y.address && x.command == y.command && x.bits == y.bits;
}

RemoteMatcher::RemoteMatcher(const RemoteLibrary& library, IFileSource* card, const std::vector<IrSignal>& heard) {
  for (const IrSignal& s : heard) {
    if (heard_.size() >= kMaxHeard) break;
    if (!isMatchable(s)) continue;
    const bool known = std::any_of(heard_.begin(), heard_.end(), [&](const IrSignal& h) { return sameCode(h, s); });
    if (!known) heard_.push_back(s);
  }
  if (heard_.empty()) {
    done_ = true;
    return;
  }

  for (const Remote& r : library.data().remotes) {
    std::vector<std::pair<std::string, const std::optional<IrSignal>*>> buttons;
    buttons.reserve(r.buttons.size());
    for (const Button& b : r.buttons) buttons.emplace_back(b.name, &b.signal);
    consider(r.name, false, r.id, std::string(), buttons);
  }

  if (card && card->available()) source_ = std::make_unique<CardFolderSource>(*card, "/irdb");
  else done_ = true;
  if (hits_.size() >= kMaxHits) done_ = true;
}

void RemoteMatcher::consider(const std::string& name, bool fromCard, Id savedId, const std::string& path,
                             const std::vector<std::pair<std::string, const std::optional<IrSignal>*>>& buttons) {
  MatchHit hit;
  uint32_t seen = 0;  // bit per heard code (kMaxHeard <= 32)
  for (size_t pos = 0; pos < buttons.size(); ++pos) {
    const std::optional<IrSignal>& sig = *buttons[pos].second;
    if (!sig || !isMatchable(*sig)) continue;
    for (size_t h = 0; h < heard_.size(); ++h) {
      if (!sameCode(heard_[h], *sig)) continue;
      seen |= 1u << h;
      if (hit.buttons.size() < kMaxButtonsPerHit) hit.buttons.push_back(MatchedButton{buttons[pos].first, pos, h});
      break;
    }
  }
  if (seen == 0) return;
  if (hits_.size() >= kMaxHits) {
    full_ = true;
    return;
  }
  for (uint32_t m = seen; m != 0; m &= m - 1) ++hit.heardMatched;
  hit.remote = name;
  hit.fromCard = fromCard;
  hit.savedId = savedId;
  hit.path = path;
  hits_.push_back(std::move(hit));
}

bool RemoteMatcher::step() {
  if (done_ || !source_) {
    done_ = true;
    return false;
  }
  RemoteView view;
  if (!source_->nextView(view)) {
    done_ = true;
    return false;
  }
  std::vector<std::pair<std::string, const std::optional<IrSignal>*>> buttons;
  buttons.reserve(view.buttons.size());
  for (const ViewButton& b : view.buttons) buttons.emplace_back(b.name, &b.signal);
  consider(view.name, true, 0, view.path, buttons);
  if (full_) done_ = true;
  return !done_;
}

void RemoteMatcher::sortByScore() {
  std::stable_sort(hits_.begin(), hits_.end(), [](const MatchHit& a, const MatchHit& b) { return a.heardMatched > b.heardMatched; });
}

}  // namespace iris
