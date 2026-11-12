#include "import/FlipperIr.h"

#include <cctype>
#include <cstdint>

#include "app/SignalBuilder.h"
#include "domain/Naming.h"
#include "domain/Protocol.h"
#include "domain/SignalValidation.h"
#include "import/FlipperSynth.h"

namespace iris {

namespace {

struct Block {
  std::string name, type, protocol, address, command, frequency, data;
  bool hasName = false;
};

std::string_view trimView(std::string_view s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
  return s;
}

bool startsWithNoCase(std::string_view s, std::string_view prefix) {
  if (s.size() < prefix.size()) return false;
  for (size_t i = 0; i < prefix.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(s[i])) != std::tolower(static_cast<unsigned char>(prefix[i]))) return false;
  }
  return true;
}

// "04 00 00 00" -> little-endian 32-bit value. 1 to 4 bytes accepted.
bool parseLeBytes(std::string_view text, uint32_t& out) {
  uint32_t value = 0;
  unsigned count = 0;
  size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && text[i] == ' ') ++i;
    if (i >= text.size()) break;
    unsigned digits = 0, byte = 0;
    while (i < text.size() && text[i] != ' ') {
      const char c = text[i++];
      unsigned d;
      if (c >= '0' && c <= '9') d = static_cast<unsigned>(c - '0');
      else if (c >= 'a' && c <= 'f') d = static_cast<unsigned>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') d = static_cast<unsigned>(c - 'A' + 10);
      else return false;
      byte = byte * 16 + d;
      if (++digits > 2) return false;
    }
    if (digits != 2 || count >= 4) return false;
    value |= static_cast<uint32_t>(byte) << (8 * count);
    ++count;
  }
  if (count == 0) return false;
  out = value;
  return true;
}

bool parseUint(std::string_view text, uint32_t& out) {
  text = trimView(text);
  if (text.empty() || text.size() > 9) return false;
  uint32_t v = 0;
  for (char c : text) {
    if (c < '0' || c > '9') return false;
    v = v * 10 + static_cast<uint32_t>(c - '0');
  }
  out = v;
  return true;
}

std::string clampName(std::string_view raw) {
  std::string name = trimName(raw);
  if (name.size() <= limits::kMaxNameBytes) return name;
  size_t cut = limits::kMaxNameBytes;
  while (cut > 0 && (static_cast<unsigned char>(name[cut]) & 0xC0) == 0x80) --cut;  // never split a UTF-8 character
  name.resize(cut);
  return trimName(name);
}

enum class Outcome { Imported, Unsupported, Invalid };

// A protocol with no decoded form here is kept as the RAW frame Flipper would send.
Outcome convertAsRaw(const Block& b, uint32_t addr, uint32_t cmd, IrSignal& out, std::string& why) {
  std::vector<uint16_t> durations;
  uint32_t carrier = 0;
  const SynthResult s = synthesizeFlipperFrame(b.protocol, addr, cmd, durations, carrier);
  if (s == SynthResult::UnknownProtocol) {
    why = "protocol " + b.protocol + " not known";
    return Outcome::Unsupported;
  }
  if (s == SynthResult::OutOfRange) {
    why = b.protocol + " value out of range";
    return Outcome::Invalid;
  }
  auto r = buildRawSignal(durations, carrier);
  if (!r.ok()) { why = r.status().message; return Outcome::Invalid; }
  out = r.value();
  out.note = "IRDB";
  return Outcome::Imported;
}

Outcome convert(const Block& b, IrSignal& out, std::string& why) {
  if (b.type == "parsed") {
    uint32_t addr = 0, cmd = 0;
    if (!parseLeBytes(b.address, addr) || !parseLeBytes(b.command, cmd)) {
      why = "address/command not readable";
      return Outcome::Invalid;
    }
    Result<IrSignal> r = Status::fail(Err::InvalidArgument, "unset");
    if (b.protocol == "NEC") {
      if (addr > 0xFF || cmd > 0xFF) { why = "NEC value out of range"; return Outcome::Invalid; }
      r = buildDecodedSignal(Protocol::Nec, addr, cmd, 32);
    } else if (b.protocol == "NECext") {
      const uint32_t cmdLow = cmd & 0xFF, cmdHigh = (cmd >> 8) & 0xFF;
      if (addr > 0xFFFF || cmd > 0xFFFF) { why = "NECext value out of range"; return Outcome::Invalid; }
      if (cmdHigh != ((~cmdLow) & 0xFF)) return convertAsRaw(b, addr, cmd, out, why);  // no inverse byte: not plain NEC
      r = buildDecodedSignal(Protocol::Nec, addr, cmdLow, 32);
    } else if (b.protocol == "Samsung32") {
      if (addr > 0xFF || cmd > 0xFF) { why = "Samsung32 value out of range"; return Outcome::Invalid; }
      r = buildDecodedSignal(Protocol::Samsung, addr, cmd, 32);
    } else if (b.protocol == "SIRC") {
      if (addr > 0x1F || cmd > 0x7F) { why = "SIRC value out of range"; return Outcome::Invalid; }
      r = buildDecodedSignal(Protocol::Sony, addr, cmd, 12);
    } else if (b.protocol == "SIRC15") {
      if (addr > 0xFF || cmd > 0x7F) { why = "SIRC15 value out of range"; return Outcome::Invalid; }
      r = buildDecodedSignal(Protocol::Sony, addr, cmd, 15);
    } else if (b.protocol == "SIRC20") {
      // Flipper: 13-bit address field = 5-bit address + 8 extended bits (low bits first); the project keeps the extended
      // bits as the upper bits of a 15-bit command.
      if (addr > 0x1FFF || cmd > 0x7F) { why = "SIRC20 value out of range"; return Outcome::Invalid; }
      r = buildDecodedSignal(Protocol::Sony, addr & 0x1F, cmd | ((addr >> 5) << 7), 20);
    } else if (b.protocol == "RC5") {
      if (addr > 0x1F || cmd > 0x3F) { why = "RC5 value out of range"; return Outcome::Invalid; }
      r = buildDecodedSignal(Protocol::Rc5, addr, cmd, 12);
    } else if (b.protocol == "RC5X") {
      // Flipper keeps the low 6 command bits; in RC5X the seventh one is always set (it travels as the field bit).
      if (addr > 0x1F || cmd > 0x7F) { why = "RC5X value out of range"; return Outcome::Invalid; }
      r = buildDecodedSignal(Protocol::Rc5, addr, (cmd & 0x3F) | 0x40, 13);
    } else {
      return convertAsRaw(b, addr, cmd, out, why);
    }
    if (!r.ok()) { why = r.status().message; return Outcome::Invalid; }
    out = r.value();
    out.note = "IRDB";
    return Outcome::Imported;
  }
  if (b.type == "raw") {
    uint32_t freq = 0;
    if (!parseUint(b.frequency, freq) || freq < limits::kMinCarrierHz || freq > limits::kMaxCarrierHz) {
      why = "RAW frequency missing or out of range";
      return Outcome::Invalid;
    }
    std::vector<uint16_t> durations;
    std::string_view d = b.data;
    size_t i = 0;
    while (i < d.size()) {
      while (i < d.size() && d[i] == ' ') ++i;
      if (i >= d.size()) break;
      size_t j = i;
      while (j < d.size() && d[j] != ' ') ++j;
      uint32_t v = 0;
      if (!parseUint(d.substr(i, j - i), v) || v == 0 || v > 65535 || durations.size() >= limits::kMaxRawEntries) {
        why = "RAW data invalid or longer than the limit";
        return Outcome::Invalid;
      }
      durations.push_back(static_cast<uint16_t>(v));
      i = j;
    }
    auto r = buildRawSignal(durations, freq);
    if (!r.ok()) { why = r.status().message; return Outcome::Invalid; }
    out = r.value();
    out.note = "IRDB";
    return Outcome::Imported;
  }
  why = "signal type '" + b.type + "' not converted";
  return Outcome::Unsupported;
}

void note(ImportReport& rep, const std::string& name, const std::string& why) {
  if (rep.firstProblem.empty()) rep.firstProblem = name + ": " + why;
}

}  // namespace

Result<ImportedRemote> parseFlipperIr(std::string_view text, size_t maxButtons, size_t rawBudget) {
  ImportedRemote result;
  bool sawFiletype = false, sawVersion = false;
  Block cur;
  bool inBlock = false;
  size_t rawUsed = 0;

  auto flush = [&]() {
    if (!inBlock) return;
    inBlock = false;
    ++result.report.seen;
    IrSignal sig;
    std::string why;
    const std::string name = clampName(cur.name);
    Outcome o = name.empty() ? Outcome::Invalid : convert(cur, sig, why);
    if (name.empty()) why = "empty name";
    if (o == Outcome::Imported) {
      const size_t raw = rawEntryCount(sig);
      if (result.buttons.size() >= maxButtons || rawUsed + raw > rawBudget) {
        ++result.report.overCapacity;
        note(result.report, name, "no room left in the remote");
      } else {
        rawUsed += raw;
        result.buttons.push_back({name, std::move(sig)});
        ++result.report.imported;
      }
    } else if (o == Outcome::Unsupported) {
      ++result.report.unsupported;
      note(result.report, name, why);
    } else {
      ++result.report.invalid;
      note(result.report, name, why);
    }
    cur = Block{};
  };

  size_t pos = 0;
  while (pos <= text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = trimView(text.substr(pos, end - pos));
    pos = end + 1;
    if (line.empty() || line.front() == '#') continue;
    const size_t colon = line.find(':');
    if (colon == std::string_view::npos) continue;
    const std::string_view key = trimView(line.substr(0, colon));
    const std::string value(trimView(line.substr(colon + 1)));

    if (!sawFiletype) {
      if (!startsWithNoCase(key, "Filetype") || value != "IR signals file")
        return Status::fail(Err::Corrupt, "not a Flipper IR signals file");
      sawFiletype = true;
      continue;
    }
    if (!sawVersion) {
      if (!startsWithNoCase(key, "Version")) return Status::fail(Err::Corrupt, "missing Version line");
      if (value != "1") return Status::fail(Err::UnsupportedVersion, "Flipper IR file version " + value);
      sawVersion = true;
      continue;
    }
    if (key == "name") {
      flush();
      inBlock = true;
      cur.name = value;
      cur.hasName = true;
    } else if (!inBlock) {
      continue;
    } else if (key == "type") cur.type = value;
    else if (key == "protocol") cur.protocol = value;
    else if (key == "address") cur.address = value;
    else if (key == "command") cur.command = value;
    else if (key == "frequency") cur.frequency = value;
    else if (key == "data") cur.data = value;
  }
  flush();
  if (!sawFiletype || !sawVersion) return Status::fail(Err::Corrupt, "not a Flipper IR signals file");
  return result;
}

}  // namespace iris
