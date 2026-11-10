#include <doctest.h>

#include "app/LibraryExport.h"
#include "app/SignalBuilder.h"
#include "import/FlipperIr.h"
#include "import/FlipperIrWriter.h"
#include "ui_helpers.h"

using namespace uihelp;

namespace {

IrSignal built(Protocol p, uint32_t addr, uint32_t cmd, uint16_t bits) { return buildDecodedSignal(p, addr, cmd, bits).value(); }

struct Back {
  Protocol protocol;
  uint32_t address, command;
  uint16_t bits;
};

}  // namespace

TEST_CASE("export: what is written reads back as the same signals") {
  const std::vector<Back> cases = {
      {Protocol::Nec, 0x04, 0x08, 32},   {Protocol::Nec, 0x1234, 0x56, 32}, {Protocol::Samsung, 0x07, 0x02, 32},
      {Protocol::Sony, 0x01, 0x15, 12},  {Protocol::Sony, 0x10, 0x2A, 15},  {Protocol::Sony, 0x01, 0x1234, 20},
  };
  std::vector<ViewButton> buttons;
  for (size_t i = 0; i < cases.size(); ++i) {
    buttons.push_back({"B" + std::to_string(i), built(cases[i].protocol, cases[i].address, cases[i].command, cases[i].bits)});
  }
  const std::vector<uint16_t> raw = {9000, 4500, 560, 560, 560, 1690, 560, 40000};
  buttons.push_back({"Crudo", buildRawSignal(raw, 36000).value()});
  buttons.push_back({"Vacio", std::nullopt});

  const WrittenIr w = writeFlipperIr(buttons);
  CHECK(w.written == cases.size() + 1);
  CHECK(w.skipped == 1);
  auto back = parseFlipperIr(w.text);
  REQUIRE(back.ok());
  REQUIRE(back.value().buttons.size() == cases.size() + 1);
  CHECK(back.value().report.unsupported == 0);
  CHECK(back.value().report.invalid == 0);
  for (size_t i = 0; i < cases.size(); ++i) {
    INFO("case " << i);
    const DecodedFrame& d = back.value().buttons[i].signal.initial.at(0).decoded;
    CHECK(d.protocol == cases[i].protocol);
    CHECK(d.address == cases[i].address);
    CHECK(d.command == cases[i].command);
    CHECK(d.bits == cases[i].bits);
  }
  const RawFrame& rf = back.value().buttons.back().signal.initial.at(0).raw;
  CHECK(rf.durationsUs == raw);
  CHECK(rf.carrierHz == 36000);
}

TEST_CASE("export: protocols the format cannot carry are counted, not written") {
  std::vector<ViewButton> buttons;
  buttons.push_back({"Sony raro", built(Protocol::Sony, 0x01, 0x7FFF, 20)});  // fits SIRC20
  auto rc5 = buildDecodedSignal(Protocol::Rc5, 1, 2, 12);
  if (rc5.ok()) buttons.push_back({"RC5", rc5.value()});
  const WrittenIr w = writeFlipperIr(buttons);
  CHECK(w.written >= 1);
  CHECK(w.text.find("Filetype: IR signals file") == 0);
}

TEST_CASE("export: file names keep ASCII and replace the rest") {
  CHECK(exportFileName("Sala") == "Sala");
  CHECK(exportFileName("TV Sala (2)") == "TV Sala (2)");
  CHECK(exportFileName("Aire acond.") == "Aire acond");
  CHECK(exportFileName("Habitaci\xC3\xB3n") == "Habitaci_n");   // one "_" per character, not per byte
  CHECK(exportFileName("a/b\\c:d") == "a_b_c_d");
  CHECK(exportFileName("") == "_");
  CHECK(exportFileName("...") == "_");
}

TEST_CASE("export: from the menus, one control, one category, everything") {
  UiRig r;
  const Id tv = r.library.addCategory("TV").value();
  const Id ac = r.library.addCategory("Aire").value();
  const Id sala = r.library.addRemote("Sala", tv).value();
  const Id pow = r.library.addButton(sala, "Power").value();
  REQUIRE(r.library.setButtonSignal(sala, pow, built(Protocol::Nec, 4, 8, 32)).ok());
  r.library.addButton(sala, "Sin senal");
  const Id cuarto = r.library.addRemote("Cuarto", tv).value();
  const Id p2 = r.library.addButton(cuarto, "Power").value();
  REQUIRE(r.library.setButtonSignal(cuarto, p2, built(Protocol::Nec, 4, 9, 32)).ok());
  const Id split = r.library.addRemote("Split", ac).value();
  const Id p3 = r.library.addButton(split, "On").value();
  REQUIRE(r.library.setButtonSignal(split, p3, built(Protocol::Samsung, 7, 2, 32)).ok());
  r.library.markClean();

  // everything, from the tools
  REQUIRE(open(r, "Herramientas IR"));
  REQUIRE(open(r, "Exportar todo a SD"));
  CHECK(r.shows("Exportados: 3"));
  REQUIRE(r.card.has("/iris/export/TV/Sala.ir"));
  REQUIRE(r.card.has("/iris/export/TV/Cuarto.ir"));
  REQUIRE(r.card.has("/iris/export/Aire/Split.ir"));
  const std::string* sala_ir = r.card.get("/iris/export/TV/Sala.ir");
  REQUIRE(sala_ir != nullptr);
  auto parsed = parseFlipperIr(*sala_ir);
  REQUIRE(parsed.ok());
  CHECK(parsed.value().buttons.size() == 1);  // the empty button is not written
  CHECK(r.library.dirty() == false);          // exporting changes nothing in the library

  // one control, from its menu
  r.longPress(Key::Left);
  REQUIRE(open(r, "Controles"));
  REQUIRE(open(r, "TV"));
  r.press(Key::Right);
  const size_t before = r.card.writes();
  REQUIRE(open(r, "Exportar a SD"));
  CHECK(r.card.writes() == before + 1);
  CHECK(r.shows("Exportados: 1"));
}

TEST_CASE("export: no card, a protected card and nothing to export are said plainly") {
  UiRig r;
  const Id tv = r.library.addCategory("TV").value();
  const Id sala = r.library.addRemote("Sala", tv).value();
  const Id pow = r.library.addButton(sala, "Power").value();
  REQUIRE(r.library.setButtonSignal(sala, pow, built(Protocol::Nec, 4, 8, 32)).ok());
  REQUIRE(open(r, "Herramientas IR"));
  r.card.setPresent(false);
  REQUIRE(open(r, "Exportar todo a SD"));
  CHECK(r.shows("Sin tarjeta SD"));
  r.card.setPresent(true);
  r.card.failWrites(true);
  r.advance(2500);
  REQUIRE(open(r, "Exportar todo a SD"));
  CHECK(r.shows("No se pudo escribir"));
  CHECK(r.card.writes() == 0);
  r.card.failWrites(false);
  r.library.removeRemote(sala);
  r.advance(2500);
  REQUIRE(open(r, "Exportar todo a SD"));
  CHECK(r.shows("Sin nada que exportar"));
}
