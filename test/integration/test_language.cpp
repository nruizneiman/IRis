#include <doctest.h>

#include <set>
#include <string>

#include "app/Settings.h"
#include "persistence/Crc32.h"
#include "ui/I18n.h"
#include "ui_helpers.h"

using namespace uihelp;

// Spanish markers that must not be on screen when English is selected: accented letters and the common Spanish words
// of the interface. A name the user typed is not checked (these screens hold none).
static bool looksSpanish(const std::string& line) {
  for (unsigned char ch : line) {
    if (ch >= 0x80) return true;  // an accented letter, "¿" or "¡"
  }
  static const char* const words[] = {"Control", "Configuracion", "Botones", "Guardar", "Borrar", "Eliminar", "Izq", "Der:",
                                      "senal", "Enviar", "Importar", "Herramientas", "Aprender", "Crear", "Volver",
                                      "Sin ", "Cree ", "Nuevo", "Nueva"};
  for (const char* w : words) {
    if (line.find(w) != std::string::npos) return true;
  }
  return false;
}

static void requireEnglish(UiRig& r, const char* where) {
  r.advance(250);  // a redraw: the screen is only painted again on a key, a change or the 200 ms refresh
  const std::string s = r.screen();
  size_t pos = 0;
  while (pos < s.size()) {
    size_t end = s.find('\n', pos);
    if (end == std::string::npos) end = s.size();
    const std::string line = s.substr(pos, end - pos);
    INFO(std::string(where) << ": " << line);
    CHECK_FALSE(looksSpanish(line));
    pos = end + 1;
  }
}

TEST_CASE("language: the catalog has no duplicate or empty entries") {
  std::set<std::string> seen;
  for (size_t i = 0; i < catalogSize(); ++i) {
    const CatalogEntry& e = catalogEntry(i);
    INFO(e.es);
    CHECK(std::string(e.es).size() > 0);
    CHECK(std::string(e.en).size() > 0);
    CHECK(seen.insert(e.es).second);
  }
}

TEST_CASE("language: translation rules") {
  CHECK(translate(Lang::Es, "Controles") == "Controles");
  CHECK(translate(Lang::En, "Controles") == "Remotes");
  CHECK(translate(Lang::En, "Botón Power") == "Button Power");           // prefix, the rest is a name
  CHECK(translate(Lang::En, "Sin guardar: SÍ") == "Unsaved: YES");       // prefix, the rest is in the catalog
  CHECK(translate(Lang::En, "3 botones") == "3 buttons");                // suffix
  CHECK(translate(Lang::En, "Mi sala") == "Mi sala");                    // unknown text is left alone
  CHECK(translate(Lang::En, "") == "");
  CHECK(std::string(langName(Lang::Es)) == "Español");
  CHECK(nextLang(nextLang(Lang::En)) == Lang::En);
}

TEST_CASE("language: English is the default and is saved with the settings") {
  Settings s;
  CHECK(s.language == Lang::En);
  s.language = Lang::Es;
  auto back = deserializeSettings(serializeSettings(s));
  REQUIRE(back.ok());
  CHECK(back.value().language == Lang::Es);
  // A file written before the language existed has no "lang" line: the default applies.
  const std::string body = "invert=0\ntimeout=15\n";
  char header[48];
  std::snprintf(header, sizeof header, "IRISCFG 1 %08lx %lu\n", static_cast<unsigned long>(crc32(body)),
                static_cast<unsigned long>(body.size()));
  auto old = deserializeSettings(std::string(header) + body);
  REQUIRE(old.ok());
  CHECK(old.value().language == kDefaultLang);
}

TEST_CASE("language: switching in Settings changes the screens at once and survives a restart") {
  UiRig r;
  CHECK(r.shows("Controles"));
  REQUIRE(open(r, "Configuración"));
  REQUIRE(open(r, "Idioma"));
  CHECK(r.shows("English"));
  CHECK(r.shows("Settings"));
  CHECK(r.settings.language == Lang::En);
  r.longPress(Key::Left);
  CHECK(r.shows("Settings"));          // the home grid names the selected icon, which is still Settings
  CHECK_FALSE(r.shows("Configuración"));
  UiRig again;                       // a new rig starts from its own flash, so this only checks the default path
  CHECK(again.settings.language == Lang::Es);
  // The stored value is what a restart reads:
  Settings stored = r.settingsStore.load();
  CHECK(stored.language == Lang::En);
}

TEST_CASE("language: the main screens read English with no Spanish left") {
  UiRig r;
  r.settings.language = Lang::En;
  r.advance(300);
  requireEnglish(r, "home");
  r.press(Key::Right);
  requireEnglish(r, "home 2");
  REQUIRE(open(r, "Herramientas IR"));
  requireEnglish(r, "tools");
  REQUIRE(open(r, "Diagnóstico HW"));
  requireEnglish(r, "diagnostics");
  r.longPress(Key::Left);
  r.longPress(Key::Left);
  REQUIRE(open(r, "Configuración"));
  requireEnglish(r, "settings");
  REQUIRE(open(r, "Almacenamiento"));
  requireEnglish(r, "storage");
  r.longPress(Key::Left);
  r.longPress(Key::Left);
  REQUIRE(open(r, "Controles"));
  requireEnglish(r, "remotes");
  REQUIRE(open(r, "+ Nueva categoría"));
  requireEnglish(r, "new category");
}

TEST_CASE("language: library, menus, learn, import and exploration screens read English") {
  UiRig r;
  r.settings.language = Lang::En;
  const Ids ids = seed(r);
  (void)ids;
  r.card.add("/irdb/TVs/LG/LG.ir", "x");
  requireEnglish(r, "home");

  REQUIRE(open(r, "Controles"));
  r.press(Key::Right);                       // category menu
  requireEnglish(r, "category menu");
  r.press(Key::Left);
  REQUIRE(open(r, "TV"));
  requireEnglish(r, "category");
  r.press(Key::Right);
  requireEnglish(r, "remote menu");
  r.press(Key::Left);
  REQUIRE(open(r, "Sala"));
  requireEnglish(r, "remote");
  r.press(Key::Right);
  requireEnglish(r, "button menu");
  REQUIRE(open(r, "Borrar señal"));
  requireEnglish(r, "delete signal confirm");
  r.longPress(Key::Left);

  REQUIRE(open(r, "Aprender señal"));
  requireEnglish(r, "learn target");
  r.longPress(Key::Left);
  REQUIRE(open(r, "Herramientas IR"));
  requireEnglish(r, "tools");
  REQUIRE(open(r, "Info del receptor"));
  requireEnglish(r, "receiver info");
  r.press(Key::Left);
  REQUIRE(open(r, "Historial arranques"));
  requireEnglish(r, "boot history");
  r.press(Key::Left);
  REQUIRE(open(r, "Exploración"));
  requireEnglish(r, "exploration form");
  r.longPress(Key::Left);

  REQUIRE(open(r, "Configuración"));
  REQUIRE(open(r, "Info del equipo"));
  requireEnglish(r, "device info");
  r.press(Key::Left);
  REQUIRE(open(r, "Prueba de botones"));
  requireEnglish(r, "button test");
  r.longPress(Key::Left);
  r.longPress(Key::Left);

  REQUIRE(open(r, "Importar"));
  requireEnglish(r, "import browser");
}
