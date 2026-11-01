#include "persistence/LibrarySerializer.h"

#include <ArduinoJson.h>

#include <cstdio>
#include <cstdlib>

#include "app/RemoteLibrary.h"
#include "domain/Limits.h"
#include "persistence/Crc32.h"

namespace iris {

namespace {

Status corrupt(const char* msg) { return Status::fail(Err::Corrupt, msg); }

// ---------------------------------------------------------------- RAW as hex text

void appendHex16(std::string& out, uint16_t v) {
  static const char* digits = "0123456789abcdef";
  out += digits[(v >> 12) & 0xF];
  out += digits[(v >> 8) & 0xF];
  out += digits[(v >> 4) & 0xF];
  out += digits[v & 0xF];
}

std::string encodeRawHex(const std::vector<uint16_t>& durations) {
  std::string s;
  s.reserve(durations.size() * 4);
  for (uint16_t d : durations) appendHex16(s, d);
  return s;
}

int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool decodeRawHex(const char* s, std::vector<uint16_t>& out) {
  const size_t n = std::char_traits<char>::length(s);
  if (n % 4 != 0 || n / 4 > limits::kMaxRawEntries) return false;
  out.reserve(n / 4);
  for (size_t i = 0; i < n; i += 4) {
    unsigned v = 0;
    for (size_t k = 0; k < 4; ++k) {
      const int h = hexValue(s[i + k]);
      if (h < 0) return false;
      v = (v << 4) | static_cast<unsigned>(h);
    }
    out.push_back(static_cast<uint16_t>(v));
  }
  return true;
}

// ---------------------------------------------------------------- writing

const char* provenanceKey(Provenance p) {
  switch (p) {
    case Provenance::Learned: return "learned";
    case Provenance::Manual: return "manual";
    case Provenance::Synthetic: return "synthetic";
  }
  return "manual";
}

const char* recognitionKey(Recognition r) {
  switch (r) {
    case Recognition::Recognized: return "recognized";
    case Recognition::Partial: return "partial";
    case Recognition::Unknown: return "unknown";
    case Recognition::Invalid: return "invalid";
  }
  return "unknown";
}

void writeRaw(JsonObject obj, const RawFrame& raw) {
  obj["hz"] = raw.carrierHz;
  obj["raw"] = encodeRawHex(raw.durationsUs);
}

void writeFrame(JsonObject obj, const Frame& f) {
  if (f.kind == Frame::Kind::Raw) {
    writeRaw(obj, f.raw);
    return;
  }
  const ProtocolInfo* info = findProtocol(f.decoded.protocol);
  obj["p"] = info ? info->key : "";
  obj["a"] = f.decoded.address;
  obj["c"] = f.decoded.command;
  obj["b"] = f.decoded.bits;
  if (f.decoded.toggle) obj["t"] = 1;
}

void writeFrames(JsonObject parent, const char* key, const std::vector<Frame>& frames) {
  JsonArray arr = parent[key].to<JsonArray>();
  for (const Frame& f : frames) writeFrame(arr.add<JsonObject>(), f);
}

// Default-valued fields are omitted (the reader restores the defaults): keeps files small.
void writeSignal(JsonObject obj, const IrSignal& s) {
  obj["prov"] = provenanceKey(s.provenance);
  obj["rec"] = recognitionKey(s.recognition);
  writeFrames(obj, "init", s.initial);
  if (!s.repeat.empty()) writeFrames(obj, "rep", s.repeat);
  if (s.repeatCount) obj["rcount"] = s.repeatCount;
  if (s.repeatGapUs) obj["gap"] = s.repeatGapUs;
  if (s.originalCapture) writeRaw(obj["orig"].to<JsonObject>(), *s.originalCapture);
  if (s.quality.empty || s.quality.truncated || s.quality.possibleNoise) {
    JsonObject q = obj["q"].to<JsonObject>();
    q["e"] = s.quality.empty ? 1 : 0;
    q["t"] = s.quality.truncated ? 1 : 0;
    q["n"] = s.quality.possibleNoise ? 1 : 0;
  }
  if (!s.note.empty()) obj["note"] = s.note;
}

void writeButtons(JsonArray arr, const std::vector<Button>& buttons) {
  for (const Button& b : buttons) {
    JsonObject bo = arr.add<JsonObject>();
    bo["id"] = b.id;
    bo["name"] = b.name;
    if (b.signal) writeSignal(bo["sig"].to<JsonObject>(), *b.signal);
  }
}

Result<std::string> finish(JsonDocument& doc, unsigned version) {
  if (doc.overflowed()) return Status::fail(Err::LimitReached, "not enough memory to serialize");
  std::string body;
  body.reserve(measureJson(doc));
  serializeJson(doc, body);
  return wrapEnvelope(version, body);
}

// ---------------------------------------------------------------- reading

bool readU32(JsonVariantConst v, uint32_t& out) {
  if (!v.is<uint32_t>()) return false;
  out = v.as<uint32_t>();
  return true;
}

// v2 stores hex text, v1 stored a number array: both are accepted here.
Status readRaw(JsonObjectConst obj, RawFrame& out) {
  if (!readU32(obj["hz"], out.carrierHz)) return corrupt("raw: bad carrier");
  JsonVariantConst raw = obj["raw"];
  if (raw.is<const char*>()) {
    if (!decodeRawHex(raw.as<const char*>(), out.durationsUs)) return corrupt("raw: bad hex data");
    return Status::success();
  }
  if (!raw.is<JsonArrayConst>()) return corrupt("raw: missing data");
  JsonArrayConst arr = raw.as<JsonArrayConst>();
  if (arr.size() > limits::kMaxRawEntries) return corrupt("raw: too long");
  out.durationsUs.reserve(arr.size());
  for (JsonVariantConst v : arr) {
    if (!v.is<uint16_t>()) return corrupt("raw: bad duration");
    out.durationsUs.push_back(v.as<uint16_t>());
  }
  return Status::success();
}

Status readFrame(JsonObjectConst obj, Frame& out) {
  if (obj["raw"].is<const char*>() || obj["raw"].is<JsonArrayConst>()) {
    out.kind = Frame::Kind::Raw;
    return readRaw(obj, out.raw);
  }
  out.kind = Frame::Kind::Decoded;
  if (!obj["p"].is<const char*>()) return corrupt("frame: missing protocol");
  const ProtocolInfo* info = findProtocolByKey(obj["p"].as<const char*>());
  if (!info) return corrupt("frame: unknown protocol");
  out.decoded.protocol = info->id;
  uint32_t bits = 0;
  if (!readU32(obj["a"], out.decoded.address) || !readU32(obj["c"], out.decoded.command) ||
      !readU32(obj["b"], bits) || bits > 0xFFFF) {
    return corrupt("frame: bad numeric field");
  }
  out.decoded.bits = static_cast<uint16_t>(bits);
  out.decoded.toggle = obj["t"].is<int>() && obj["t"].as<int>() != 0;
  return Status::success();
}

Status readFrames(JsonObjectConst parent, const char* key, bool required, std::vector<Frame>& out) {
  if (!parent[key].is<JsonArrayConst>()) return required ? corrupt("signal: missing frame list") : Status::success();
  JsonArrayConst arr = parent[key].as<JsonArrayConst>();
  if (arr.size() > limits::kMaxFramesPerList) return corrupt("signal: too many frames");
  for (JsonObjectConst f : arr) {
    Frame frame;
    Status st = readFrame(f, frame);
    if (!st.ok()) return st;
    out.push_back(std::move(frame));
  }
  return Status::success();
}

bool parseProvenance(const char* s, Provenance& out) {
  const std::string v = s;
  if (v == "learned") out = Provenance::Learned;
  else if (v == "manual") out = Provenance::Manual;
  else if (v == "synthetic") out = Provenance::Synthetic;
  else return false;
  return true;
}

bool parseRecognition(const char* s, Recognition& out) {
  const std::string v = s;
  if (v == "recognized") out = Recognition::Recognized;
  else if (v == "partial") out = Recognition::Partial;
  else if (v == "unknown") out = Recognition::Unknown;
  else if (v == "invalid") out = Recognition::Invalid;
  else return false;
  return true;
}

Status readSignal(JsonObjectConst obj, IrSignal& s) {
  if (!obj["prov"].is<const char*>() || !parseProvenance(obj["prov"].as<const char*>(), s.provenance)) {
    return corrupt("signal: bad provenance");
  }
  if (!obj["rec"].is<const char*>() || !parseRecognition(obj["rec"].as<const char*>(), s.recognition)) {
    return corrupt("signal: bad recognition");
  }
  Status st = readFrames(obj, "init", true, s.initial);
  if (!st.ok()) return st;
  st = readFrames(obj, "rep", false, s.repeat);
  if (!st.ok()) return st;
  uint32_t rcount = 0;
  if (!obj["rcount"].isNull() && (!readU32(obj["rcount"], rcount) || rcount > 255)) return corrupt("signal: bad repeat count");
  s.repeatCount = static_cast<uint8_t>(rcount);
  if (!obj["gap"].isNull() && !readU32(obj["gap"], s.repeatGapUs)) return corrupt("signal: bad gap");
  if (obj["orig"].is<JsonObjectConst>()) {
    RawFrame raw;
    st = readRaw(obj["orig"].as<JsonObjectConst>(), raw);
    if (!st.ok()) return st;
    s.originalCapture = std::move(raw);
  }
  if (obj["q"].is<JsonObjectConst>()) {
    JsonObjectConst q = obj["q"];
    s.quality.empty = q["e"].as<int>() != 0;
    s.quality.truncated = q["t"].as<int>() != 0;
    s.quality.possibleNoise = q["n"].as<int>() != 0;
  }
  if (obj["note"].is<const char*>()) s.note = obj["note"].as<const char*>();
  return Status::success();
}

Status readButtons(JsonArrayConst arr, std::vector<Button>& out) {
  if (arr.size() > limits::kMaxButtonsPerRemote) return corrupt("too many buttons");
  for (JsonObjectConst bo : arr) {
    Button b;
    if (!readU32(bo["id"], b.id) || !bo["name"].is<const char*>()) return corrupt("bad button");
    b.name = bo["name"].as<const char*>();
    if (bo["sig"].is<JsonObjectConst>()) {
      IrSignal sig;
      Status st = readSignal(bo["sig"].as<JsonObjectConst>(), sig);
      if (!st.ok()) return st;
      b.signal = std::move(sig);
    }
    out.push_back(std::move(b));
  }
  return Status::success();
}

Status readCategories(JsonArrayConst cats, std::vector<Category>& out) {
  if (cats.size() > limits::kMaxCategories) return corrupt("too many categories");
  for (JsonObjectConst c : cats) {
    Category cat;
    if (!readU32(c["id"], cat.id) || !c["name"].is<const char*>()) return corrupt("bad category");
    cat.name = c["name"].as<const char*>();
    out.push_back(std::move(cat));
  }
  return Status::success();
}

Status parseBody(const Envelope& env, JsonDocument& doc) {
  DeserializationError err = deserializeJson(doc, env.body.data(), env.body.size());
  if (err == DeserializationError::NoMemory) {
    return Status::fail(Err::LimitReached, "not enough memory to read the file");  // NOT a sign of damage
  }
  if (err) return corrupt("invalid JSON");
  return Status::success();
}

}  // namespace

// ---------------------------------------------------------------- envelope

std::string wrapEnvelope(unsigned version, const std::string& body) {
  char header[64];
  std::snprintf(header, sizeof(header), "IRIS %u %08lx %lu\n", version, static_cast<unsigned long>(crc32(body)),
                static_cast<unsigned long>(body.size()));
  return std::string(header) + body;
}

Result<Envelope> openEnvelope(const std::string& text) {
  const size_t nl = text.find('\n');
  if (nl == std::string::npos || nl > 48) return corrupt("missing header");
  unsigned version = 0;
  unsigned long crc = 0, length = 0;
  char magic[8] = {0};
  if (std::sscanf(text.substr(0, nl).c_str(), "%7s %u %lx %lu", magic, &version, &crc, &length) != 4 ||
      std::string(magic) != "IRIS") {
    return corrupt("bad header");
  }
  if (version == 0) return corrupt("bad version");
  const std::string_view body(text.data() + nl + 1, text.size() - nl - 1);
  if (body.size() != length) return corrupt("length mismatch (truncated or extended)");
  if (crc32(body) != static_cast<uint32_t>(crc)) return corrupt("checksum mismatch");
  if (version > kFormatVersion) return Status::fail(Err::UnsupportedVersion, "file was written by a newer firmware");
  Envelope env;
  env.version = version;
  env.body = body;
  return env;
}

Status checkEnvelope(const std::string& text) {
  Result<Envelope> env = openEnvelope(text);
  return env.ok() ? Status::success() : env.status();
}

// ---------------------------------------------------------------- format 2

Result<std::string> serializeIndex(const IndexData& index) {
  JsonDocument doc;
  doc["next"] = index.nextId;
  doc["gen"] = index.generation;
  JsonArray cats = doc["cats"].to<JsonArray>();
  for (const Category& c : index.categories) {
    JsonObject o = cats.add<JsonObject>();
    o["id"] = c.id;
    o["name"] = c.name;
  }
  JsonArray remotes = doc["remotes"].to<JsonArray>();
  for (const IndexEntry& e : index.remotes) {
    JsonObject o = remotes.add<JsonObject>();
    o["id"] = e.id;
    o["name"] = e.name;
    o["cat"] = e.categoryId;
    o["file"] = e.fileKey;
    o["crc"] = e.fileCrc;
  }
  return finish(doc, kFormatVersion);
}

Result<IndexData> deserializeIndex(const std::string& text) {
  Result<Envelope> env = openEnvelope(text);
  if (!env.ok()) return env.status();
  if (env.value().version != kFormatVersion) return corrupt("not an index file");
  JsonDocument doc;
  Status st = parseBody(env.value(), doc);
  if (!st.ok()) return st;

  IndexData index;
  if (!readU32(doc["next"], index.nextId) || !readU32(doc["gen"], index.generation)) return corrupt("missing counters");
  if (!doc["cats"].is<JsonArrayConst>() || !doc["remotes"].is<JsonArrayConst>()) return corrupt("missing sections");
  st = readCategories(doc["cats"].as<JsonArrayConst>(), index.categories);
  if (!st.ok()) return st;
  JsonArrayConst remotes = doc["remotes"].as<JsonArrayConst>();
  if (remotes.size() > limits::kMaxRemotes) return corrupt("too many remotes");
  for (JsonObjectConst o : remotes) {
    IndexEntry e;
    if (!readU32(o["id"], e.id) || !readU32(o["cat"], e.categoryId) || !readU32(o["crc"], e.fileCrc) ||
        !o["name"].is<const char*>() || !o["file"].is<const char*>()) {
      return corrupt("bad remote entry");
    }
    e.name = o["name"].as<const char*>();
    e.fileKey = o["file"].as<const char*>();
    if (e.fileKey.empty() || e.fileKey.size() > 24) return corrupt("bad remote file key");
    index.remotes.push_back(std::move(e));
  }
  return index;
}

Result<std::string> serializeRemoteFile(const Remote& remote) {
  JsonDocument doc;
  doc["id"] = remote.id;
  writeButtons(doc["buttons"].to<JsonArray>(), remote.buttons);
  return finish(doc, kFormatVersion);
}

Result<Remote> deserializeRemoteFile(const std::string& text) {
  Result<Envelope> env = openEnvelope(text);
  if (!env.ok()) return env.status();
  if (env.value().version != kFormatVersion) return corrupt("not a remote file");
  JsonDocument doc;
  Status st = parseBody(env.value(), doc);
  if (!st.ok()) return st;
  Remote r;
  if (!readU32(doc["id"], r.id) || !doc["buttons"].is<JsonArrayConst>()) return corrupt("bad remote file");
  st = readButtons(doc["buttons"].as<JsonArrayConst>(), r.buttons);
  if (!st.ok()) return st;
  return r;
}

size_t estimateRemoteFileBytes(const Remote& remote) {
  size_t n = 96;
  for (const Button& b : remote.buttons) {
    n += 48 + b.name.size();
    if (!b.signal) continue;
    n += 120 + b.signal->note.size();
    auto frames = [&](const std::vector<Frame>& v) {
      for (const Frame& f : v) n += f.kind == Frame::Kind::Raw ? 40 + f.raw.durationsUs.size() * 4 : 64;
    };
    frames(b.signal->initial);
    frames(b.signal->repeat);
    if (b.signal->originalCapture) n += 40 + b.signal->originalCapture->durationsUs.size() * 4;
  }
  return n;
}

// ---------------------------------------------------------------- format 1 (read only)

Result<LibraryData> deserializeLegacyLibrary(const std::string& text) {
  Result<Envelope> env = openEnvelope(text);
  if (!env.ok()) return env.status();
  if (env.value().version != kLegacyFormatVersion) return corrupt("not a legacy file");
  JsonDocument doc;
  Status st = parseBody(env.value(), doc);
  if (!st.ok()) return st;

  LibraryData data;
  if (!readU32(doc["next"], data.nextId)) return corrupt("missing next id");
  if (!doc["cats"].is<JsonArrayConst>() || !doc["remotes"].is<JsonArrayConst>()) return corrupt("missing sections");
  st = readCategories(doc["cats"].as<JsonArrayConst>(), data.categories);
  if (!st.ok()) return st;
  JsonArrayConst remotes = doc["remotes"].as<JsonArrayConst>();
  if (remotes.size() > limits::kMaxRemotes) return corrupt("too many remotes");
  for (JsonObjectConst ro : remotes) {
    Remote r;
    if (!readU32(ro["id"], r.id) || !readU32(ro["cat"], r.categoryId) || !ro["name"].is<const char*>() ||
        !ro["buttons"].is<JsonArrayConst>()) {
      return corrupt("bad remote");
    }
    r.name = ro["name"].as<const char*>();
    st = readButtons(ro["buttons"].as<JsonArrayConst>(), r.buttons);
    if (!st.ok()) return st;
    data.remotes.push_back(std::move(r));
  }
  st = validateLibraryData(data);
  if (!st.ok()) return st.code == Err::Corrupt ? st : corrupt(st.message.c_str());
  return data;
}

}  // namespace iris
