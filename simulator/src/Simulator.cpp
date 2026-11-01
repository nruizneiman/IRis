#include "Simulator.h"

#include <cstdlib>
#include <sstream>

#include "Fixtures.h"
#include "app/SignalBuilder.h"
#include "domain/SignalValidation.h"
#include "persistence/LibrarySerializer.h"

namespace iris::sim {

namespace {

std::vector<std::string> tokenize(const std::string& line) {
  std::istringstream in(line);
  std::vector<std::string> out;
  std::string t;
  while (in >> t) out.push_back(t);
  return out;
}

// Rest of the line after `skip` tokens (names may contain spaces).
std::string tail(const std::vector<std::string>& a, size_t skip) {
  std::string s;
  for (size_t i = skip; i < a.size(); ++i) s += (s.empty() ? "" : " ") + a[i];
  return s;
}

bool parseU32(const std::string& s, uint32_t& out) {
  if (s.empty()) return false;
  char* end = nullptr;
  const unsigned long long v = std::strtoull(s.c_str(), &end, 0);
  if (*end != '\0' || v > 0xFFFFFFFFull) return false;
  out = static_cast<uint32_t>(v);
  return true;
}

std::string err(const Status& st) { return std::string("ERROR ") + errName(st.code) + ": " + st.message; }

const char* recName(Recognition r) {
  switch (r) {
    case Recognition::Recognized: return "recognized";
    case Recognition::Partial: return "partial";
    case Recognition::Unknown: return "unknown";
    case Recognition::Invalid: return "invalid";
  }
  return "?";
}

std::string describeFrame(const Frame& f) {
  std::ostringstream o;
  if (f.kind == Frame::Kind::Raw) {
    o << "raw[" << f.raw.durationsUs.size() << "] carrier="
      << (f.raw.carrierHz ? std::to_string(f.raw.carrierHz) : std::string("unknown"));
  } else {
    const ProtocolInfo* p = findProtocol(f.decoded.protocol);
    o << (p ? p->displayName : "?") << " addr=0x" << std::hex << f.decoded.address << " cmd=0x" << f.decoded.command
      << std::dec << " bits=" << f.decoded.bits;
  }
  return o.str();
}

std::string describe(const IrSignal& s) {
  std::ostringstream o;
  o << recName(s.recognition) << ", frames: ";
  for (size_t i = 0; i < s.initial.size(); ++i) o << (i ? "; " : "") << describeFrame(s.initial[i]);
  if (s.repeatCount) o << " | repeat x" << int(s.repeatCount);
  o << (s.originalCapture ? " | original RAW kept (" + std::to_string(s.originalCapture->durationsUs.size()) + ")" : "");
  if (s.quality.truncated) o << " | TRUNCATED";
  if (s.quality.possibleNoise) o << " | POSSIBLE NOISE";
  return o.str();
}

}  // namespace

Simulator::Simulator() = default;

void Simulator::step() { rig_.step(); }

std::string Simulator::help() const {
  return "Commands:\n"
         "  status | list | log | help\n"
         "  advance <ms>                       let time pass (10 ms main-loop steps)\n"
         "  cat add <name> | remote add <catId> <name> | button add <remoteId> <name>\n"
         "  learn <timeoutMs> | learn-cancel | learn-show | learn-save <remoteId> <buttonId>\n"
         "  inject nec <addr> <cmd> | inject unknown | inject noise | inject empty   (SYNTHETIC captures)\n"
         "  manual <remoteId> <buttonId> <nec|samsung|sony|rc5|jvc> <addr> <cmd> <bits>\n"
         "  manual-raw <remoteId> <buttonId> <carrierHz|0> <d1> <d2> ...\n"
         "  send <remoteId> <buttonId> | cancel | txlog\n"
         "  explore <proto> <bits> <addrFirst> <addrLast> <cmdFirst> <cmdLast> <max> <gapMs>\n"
         "  explore-confirm | explore-start | explore-stop | explore-status | explore-log | explore-save\n"
         "  fault rx-start|tx-submit|tx-mid|write|read on|off\n"
         "  save | reboot | corrupt\n"
         "All hardware is simulated; nothing here proves behaviour on the real device.";
}

std::string Simulator::status() const {
  std::ostringstream o;
  o << "t=" << clock_.nowMs() << "ms | library: " << library_.data().categories.size() << " categories, "
    << library_.data().remotes.size() << " remotes" << (library_.dirty() ? " (UNSAVED CHANGES)" : "")
    << " | learn=";
  switch (learn_.state()) {
    case LearnState::Idle: o << "idle"; break;
    case LearnState::Armed: o << "armed"; break;
    case LearnState::Captured: o << "captured"; break;
    case LearnState::Rejected: o << "rejected"; break;
    case LearnState::TimedOut: o << "timed-out"; break;
    case LearnState::Failed: o << "failed"; break;
  }
  o << " | tx=";
  switch (tx_.state()) {
    case TxState::Idle: o << "idle"; break;
    case TxState::Sending: o << "sending"; break;
    case TxState::Cancelling: o << "cancelling"; break;
    case TxState::Failed: o << "FAILED"; break;
  }
  o << " | battery=" << (battery_.readMillivolts() ? std::to_string(*battery_.readMillivolts()) + " mV" : "not available");
  return o.str();
}

std::string Simulator::list() const {
  std::ostringstream o;
  const LibraryData& d = library_.data();
  if (d.categories.empty()) return "(library is empty)";
  for (const Category& c : d.categories) {
    o << "[" << c.id << "] " << c.name << "\n";
    for (const Remote& r : d.remotes) {
      if (r.categoryId != c.id) continue;
      o << "  [" << r.id << "] " << r.name << "\n";
      for (const Button& b : r.buttons) {
        o << "    [" << b.id << "] " << b.name << " - " << (b.signal ? describe(*b.signal) : "no signal") << "\n";
      }
    }
  }
  std::string s = o.str();
  if (!s.empty()) s.pop_back();
  return s;
}

std::string Simulator::cmdAdvance(const std::vector<std::string>& a) {
  uint32_t ms = 0;
  if (a.size() != 2 || !parseU32(a[1], ms) || ms > 600000) return "usage: advance <ms 0..600000>";
  for (uint32_t done = 0; done < ms;) {
    const uint32_t chunk = std::min<uint32_t>(10, ms - done);
    clock_.advanceMs(chunk);
    done += chunk;
    step();
  }
  step();
  return status();
}

std::string Simulator::cmdInject(const std::vector<std::string>& a) {
  ReceivedCapture c;
  c.timestampMs = clock_.nowMs();
  if (a.size() >= 2 && a[1] == "nec") {
    uint32_t addr, cmd;
    if (a.size() != 4 || !parseU32(a[2], addr) || !parseU32(a[3], cmd) || addr > 0xFF || cmd > 0xFF) {
      return "usage: inject nec <addr 0..255> <cmd 0..255>";
    }
    const uint32_t data = addr | ((~addr & 0xFFu) << 8) | (cmd << 16) | ((~cmd & 0xFFu) << 24);
    c.raw.durationsUs = testkit::syntheticNecFrame(data);
    DecodedFrame d;  // stands in for the IR library's decode result
    d.protocol = Protocol::Nec;
    d.address = addr;
    d.command = cmd;
    d.bits = 32;
    c.decoded = d;
  } else if (a.size() == 2 && a[1] == "unknown") {
    c.raw.durationsUs = testkit::syntheticUnknownPattern();
  } else if (a.size() == 2 && a[1] == "noise") {
    c.raw.durationsUs = testkit::syntheticNoiseCapture();
  } else if (a.size() == 2 && a[1] == "empty") {
    c.raw.durationsUs = testkit::emptyCapture();
  } else {
    return "usage: inject nec <addr> <cmd> | unknown | noise | empty";
  }
  rx_.push(std::move(c));
  step();
  return "injected synthetic capture. " + status();
}

std::string Simulator::cmdLearnSave(const std::vector<std::string>& a) {
  Id remote, button;
  uint32_t r, b;
  if (a.size() != 3 || !parseU32(a[1], r) || !parseU32(a[2], b)) return "usage: learn-save <remoteId> <buttonId>";
  remote = r;
  button = b;
  if (learn_.state() != LearnState::Captured) return "ERROR nothing captured to save";
  Status st = library_.setButtonSignal(remote, button, *learn_.result());
  if (!st.ok()) return err(st);  // candidate is kept so the user can retry or pick another button
  learn_.take();
  return "saved to button (not persisted until 'save')";
}

std::string Simulator::cmdManual(const std::vector<std::string>& a) {
  uint32_t r, b, addr, cmd, bits;
  if (a.size() != 7 || !parseU32(a[1], r) || !parseU32(a[2], b) || !parseU32(a[4], addr) || !parseU32(a[5], cmd) ||
      !parseU32(a[6], bits) || bits > 0xFFFF) {
    return "usage: manual <remoteId> <buttonId> <protocol> <addr> <cmd> <bits>";
  }
  const ProtocolInfo* p = findProtocolByKey(a[3]);
  if (!p) return "ERROR unknown protocol key '" + a[3] + "'";
  auto sig = buildDecodedSignal(p->id, addr, cmd, static_cast<uint16_t>(bits));
  if (!sig.ok()) return err(sig.status());
  Status st = library_.setButtonSignal(r, b, sig.value());
  return st.ok() ? "manual signal stored (not persisted until 'save')" : err(st);
}

std::string Simulator::cmdManualRaw(const std::vector<std::string>& a) {
  uint32_t r, b, carrier;
  if (a.size() < 5 || !parseU32(a[1], r) || !parseU32(a[2], b) || !parseU32(a[3], carrier)) {
    return "usage: manual-raw <remoteId> <buttonId> <carrierHz|0> <d1> <d2> ...";
  }
  std::vector<uint16_t> d;
  for (size_t i = 4; i < a.size(); ++i) {
    uint32_t v;
    if (!parseU32(a[i], v) || v > 0xFFFF) return "ERROR bad duration '" + a[i] + "'";
    d.push_back(static_cast<uint16_t>(v));
  }
  auto sig = buildRawSignal(d, carrier);
  if (!sig.ok()) return err(sig.status());
  Status st = library_.setButtonSignal(r, b, sig.value());
  return st.ok() ? "raw signal stored (not persisted until 'save')" : err(st);
}

std::string Simulator::cmdSend(const std::vector<std::string>& a) {
  uint32_t r, b;
  if (a.size() != 3 || !parseU32(a[1], r) || !parseU32(a[2], b)) return "usage: send <remoteId> <buttonId>";
  const Button* btn = library_.findButton(r, b);
  if (!btn) return "ERROR button not found";
  if (!btn->signal) return "ERROR button has no signal";
  Status st = transmit_.send(*btn->signal);
  return st.ok() ? "sending (simulated). " + status() : err(st);
}

std::string Simulator::cmdExplore(const std::vector<std::string>& a) {
  uint32_t bits, a1, a2, c1, c2, mx, gap;
  if (a.size() != 9 || !parseU32(a[2], bits) || !parseU32(a[3], a1) || !parseU32(a[4], a2) || !parseU32(a[5], c1) ||
      !parseU32(a[6], c2) || !parseU32(a[7], mx) || !parseU32(a[8], gap) || bits > 0xFFFF) {
    return "usage: explore <proto> <bits> <addrFirst> <addrLast> <cmdFirst> <cmdLast> <max> <gapMs>";
  }
  const ProtocolInfo* p = findProtocolByKey(a[1]);
  if (!p) return "ERROR unknown protocol key '" + a[1] + "'";
  ExplorationPlan plan;
  plan.protocol = p->id;
  plan.bits = static_cast<uint16_t>(bits);
  plan.addressFirst = a1; plan.addressLast = a2;
  plan.commandFirst = c1; plan.commandLast = c2;
  plan.maxTransmissions = mx;
  plan.gapMs = gap;
  Status st = explore_.prepare(plan);
  if (!st.ok()) return err(st);
  if (explore_.state() == ExplorationState::AwaitingConfirmation) {
    return std::to_string(explore_.totalCount()) + " codes: this is a long run, type 'explore-confirm' then 'explore-start'. "
           "The emitter cannot tell whether the target reacted.";
  }
  return std::to_string(explore_.totalCount()) + " codes ready, type 'explore-start'";
}

std::string Simulator::cmdFault(const std::vector<std::string>& a) {
  if (a.size() < 2) return "usage: fault rx-start|tx-submit|tx-mid|write|read on|off";
  const bool on = !(a.size() >= 3 && a[2] == "off");
  if (a[1] == "rx-start") rx_.setFailStart(on);
  else if (a[1] == "tx-submit") { if (on) tx_.failNextSubmit(); }
  else if (a[1] == "tx-mid") { if (on) tx_.faultDuringNextService(); }
  else if (a[1] == "write") flash_.failWritesAfter(on ? 0 : -1);
  else if (a[1] == "read") flash_.setFailReads(on);
  else return "ERROR unknown fault '" + a[1] + "'";
  return "fault " + a[1] + (on ? " ON" : " OFF");
}

std::string Simulator::cmdKey(const std::vector<std::string>& a) {
  static const struct { const char* name; Key key; } keys[] = {
      {"up", Key::Up}, {"down", Key::Down}, {"left", Key::Left}, {"right", Key::Right}, {"ok", Key::Ok}};
  if (a.size() < 2) return "usage: key <up|down|left|right|ok> [long]  (goes through the real debouncer)";
  for (const auto& k : keys) {
    if (a[1] != k.name) continue;
    if (a.size() >= 3 && a[2] == "long") rig_.longPress(k.key);
    else rig_.press(k.key);
    return rig_.screen();
  }
  return "ERROR unknown key '" + a[1] + "'";
}

std::string Simulator::cmdReboot() {
  learn_.cancel();
  explore_.reset();
  auto loaded = repo_.load();
  if (!loaded.ok()) {
    const bool damage = loaded.status().code == Err::Corrupt;
    rig_.ctx.persistEnabled = damage;  // anything else (read error, newer data): never overwrite what was not read
    return "boot: " + err(loaded.status()) + " (library NOT replaced, files untouched" +
           std::string(damage ? ")" : ", saving DISABLED)");
  }
  rig_.ctx.persistEnabled = true;
  library_ = std::move(loaded.value().library);
  const char* src = loaded.value().source == LoadSource::Primary  ? "primary file"
                    : loaded.value().source == LoadSource::Backup ? "BACKUP file (primary unreadable)"
                    : loaded.value().source == LoadSource::Legacy ? "legacy format-1 file (migrates on next save)"
                                                                  : "nothing stored, empty library";
  std::string out = std::string("boot: loaded from ") + src;
  for (const std::string& name : loaded.value().lostRemotes) out += std::string(1, '\n') + "LOST (unreadable file): " + name;
  return out;
}

std::string Simulator::execute(const std::string& line) {
  const std::vector<std::string> a = tokenize(line);
  if (a.empty()) return "";
  const std::string& c = a[0];

  if (c == "help") return help();
  if (c == "key") return cmdKey(a);
  if (c == "screen") return rig_.screen();
  if (c == "status") return status();
  if (c == "list") return list();
  if (c == "advance") return cmdAdvance(a);

  if (c == "cat" && a.size() >= 3 && a[1] == "add") {
    auto r = library_.addCategory(tail(a, 2));
    return r.ok() ? "category id " + std::to_string(r.value()) : err(r.status());
  }
  if (c == "remote" && a.size() >= 4 && a[1] == "add") {
    uint32_t cat;
    if (!parseU32(a[2], cat)) return "usage: remote add <catId> <name>";
    auto r = library_.addRemote(tail(a, 3), cat);
    return r.ok() ? "remote id " + std::to_string(r.value()) : err(r.status());
  }
  if (c == "button" && a.size() >= 4 && a[1] == "add") {
    uint32_t rem;
    if (!parseU32(a[2], rem)) return "usage: button add <remoteId> <name>";
    auto r = library_.addButton(rem, tail(a, 3));
    return r.ok() ? "button id " + std::to_string(r.value()) : err(r.status());
  }

  if (c == "learn" && a.size() == 2) {
    uint32_t t;
    if (!parseU32(a[1], t)) return "usage: learn <timeoutMs>";
    Status st = learn_.start(t);
    step();
    return st.ok() ? "waiting for a signal... " + status() : err(st);
  }
  if (c == "learn-cancel") { learn_.cancel(); return "capture cancelled"; }
  if (c == "learn-show") {
    switch (learn_.state()) {
      case LearnState::Captured: return "captured: " + describe(*learn_.result());
      case LearnState::Rejected: return "rejected: " + learn_.lastStatus().message;
      case LearnState::TimedOut: return "timed out";
      case LearnState::Failed: return "failed: " + learn_.lastStatus().message;
      case LearnState::Armed: return "still waiting";
      case LearnState::Idle: return "idle";
    }
  }
  if (c == "learn-save") return cmdLearnSave(a);
  if (c == "inject") return cmdInject(a);
  if (c == "manual") return cmdManual(a);
  if (c == "manual-raw") return cmdManualRaw(a);
  if (c == "send") return cmdSend(a);
  if (c == "cancel") { transmit_.cancel(); return "cancel requested (takes effect between frames)"; }
  if (c == "txlog") {
    std::ostringstream o;
    for (const auto& h : tx_.history()) {
      o << "t=" << h.startMs << "ms frames " << h.framesSent << "/" << h.framesPlanned
        << (h.completed ? " completed" : "") << (h.cancelled ? " CANCELLED" : "") << (h.faulted ? " FAULT" : "") << "\n";
    }
    std::string s = o.str();
    return s.empty() ? "(nothing sent)" : s.substr(0, s.size() - 1);
  }

  if (c == "explore") return cmdExplore(a);
  if (c == "explore-confirm") { Status st = explore_.confirm(); return st.ok() ? "confirmed" : err(st); }
  if (c == "explore-start") { Status st = explore_.start(); return st.ok() ? "running" : err(st); }
  if (c == "explore-stop") { explore_.stop(); return "stop requested"; }
  if (c == "explore-status") {
    static const char* names[] = {"idle", "awaiting-confirmation", "ready", "running", "stopped", "completed", "failed"};
    return std::string("exploration ") + names[static_cast<int>(explore_.state())] + " " +
           std::to_string(explore_.sentCount()) + "/" + std::to_string(explore_.totalCount());
  }
  if (c == "explore-log") { std::string s = explore_.logText(); return s.empty() ? "(empty)" : s.substr(0, s.size() - 1); }
  if (c == "explore-save") { Status st = explore_.saveLog(flash_, "explore.log"); return st.ok() ? "log saved" : err(st); }

  if (c == "fault") return cmdFault(a);
  if (c == "save") {
    Status st = repo_.save(library_);
    if (st.ok()) library_.markClean();
    return st.ok() ? "saved" : err(st);
  }
  if (c == "reboot") return cmdReboot();
  if (c == "dump") {
    auto raw = flash_.read(a.size() > 1 ? a[1] : std::string(LibraryRepository::indexKey()));
    return raw.ok() ? raw.value() : "ERROR " + raw.status().message;
  }
  if (c == "corrupt") {
    if (!flash_.exists(LibraryRepository::indexKey())) return "ERROR no index file";
    flash_.flipByte(LibraryRepository::indexKey(), 30);
    return "index file damaged (one byte flipped)";
  }
  if (c == "log") {
    std::ostringstream o;
    for (const auto& e : log_.entries()) o << e.timeMs << "ms [" << e.tag << "] " << e.message << "\n";
    std::string s = o.str();
    return s.empty() ? "(no events)" : s.substr(0, s.size() - 1);
  }
  return "ERROR unknown command '" + c + "' (try 'help')";
}

}  // namespace iris::sim
