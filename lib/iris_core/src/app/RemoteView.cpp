#include "app/RemoteView.h"

#include <cctype>

#include "domain/Limits.h"
#include "domain/Naming.h"
#include "ui/Utf8.h"

namespace iris {

Result<RemoteView> viewSavedRemote(const RemoteLibrary& library, Id remoteId) {
  const Remote* r = library.findRemote(remoteId);
  if (!r) return Status::fail(Err::NotFound, "remote not found");
  RemoteView v;
  v.name = r->name;
  v.savedId = r->id;
  v.buttons.reserve(r->buttons.size());
  for (const Button& b : r->buttons) v.buttons.push_back({b.name, b.signal});
  return v;
}

bool endsWithIr(const std::string& n) { return n.size() > 3 && n.compare(n.size() - 3, 3, ".ir") == 0; }

std::string remoteNameFromFile(const std::string& file) {
  std::string n = file;
  if (endsWithIr(n)) n.resize(n.size() - 3);
  for (char& ch : n) {
    if (ch == '_') ch = ' ';
  }
  return trimName(utf8Truncate(n, limits::kMaxNameBytes));
}

Result<RemoteView> viewCardRemote(IFileSource& files, const std::string& path) {
  if (!files.available()) return Status::fail(Err::Unavailable, "no card");
  Result<std::string> text = files.readText(path, limits::kMaxCardFileBytes);
  if (!text.ok()) return text.status();
  Result<ImportedRemote> parsed = parseFlipperIr(text.value(), limits::kMaxCardButtons, limits::kMaxCardRawEntries);
  if (!parsed.ok()) return parsed.status();
  RemoteView v;
  const size_t slash = path.rfind('/');
  v.name = remoteNameFromFile(slash == std::string::npos ? path : path.substr(slash + 1));
  v.fromCard = true;
  v.path = path;
  v.report = parsed.value().report;
  v.buttons.reserve(parsed.value().buttons.size());
  for (ImportedButton& b : parsed.value().buttons) v.buttons.push_back({std::move(b.name), std::move(b.signal)});
  return v;
}

namespace {

// Lower-case letters and digits only, so "Power_Off", "power off" and "POWER-OFF" compare equal.
std::string squash(const std::string& s) {
  std::string out;
  for (char ch : s) {
    if (std::isalnum(static_cast<unsigned char>(ch))) out += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return out;
}

bool oneOf(const std::string& s, std::initializer_list<const char*> list) {
  for (const char* x : list) {
    if (s == x) return true;
  }
  return false;
}

}  // namespace

const ViewButton* findPowerButton(const RemoteView& view, PowerWanted wanted) {
  const ViewButton* toggle = nullptr;
  for (const ViewButton& b : view.buttons) {
    if (!b.signal) continue;
    const std::string n = squash(b.name);
    if (wanted == PowerWanted::Off && oneOf(n, {"poweroff", "off", "apagar", "turnoff", "pwroff", "standbyoff", "sleep"})) return &b;
    if (wanted == PowerWanted::On && oneOf(n, {"poweron", "on", "encender", "turnon", "pwron"})) return &b;
    if (!toggle && oneOf(n, {"power", "pwr", "onoff", "standby", "stby", "powertoggle", "encenderapagar", "apagarencender"})) {
      toggle = &b;
    }
  }
  return toggle;
}

}  // namespace iris
