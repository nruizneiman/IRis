#include <doctest.h>

#include <fstream>
#include <sstream>
#include <string>

#include "domain/Naming.h"
#include "import/FlipperIr.h"

using namespace iris;

namespace {

std::string readFixture(const char* name) {
  std::ifstream in(std::string(IRIS_FIXTURE_DIR) + "/" + name, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

const char* kHeader = "Filetype: IR signals file\nVersion: 1\n#\n";

std::string parsedBlock(const std::string& name, const std::string& proto, const std::string& addr,
                        const std::string& cmd) {
  return "# \nname: " + name + "\ntype: parsed\nprotocol: " + proto + "\naddress: " + addr + "\ncommand: " + cmd + "\n";
}

const DecodedFrame& decodedOf(const ImportedButton& b) { return b.signal.initial.at(0).decoded; }

}  // namespace

TEST_CASE("real Flipper-IRDB file (LG TV) converts to six NEC buttons") {
  auto r = parseFlipperIr(readFixture("irdb_LG.ir"));
  REQUIRE(r.ok());
  const auto& rem = r.value();
  CHECK(rem.report.seen == 6);
  CHECK(rem.report.imported == 6);
  CHECK(rem.report.unsupported == 0);
  CHECK(rem.report.invalid == 0);
  REQUIRE(rem.buttons.size() == 6);
  CHECK(rem.buttons[0].name == "Power");
  CHECK(decodedOf(rem.buttons[0]).protocol == Protocol::Nec);
  CHECK(decodedOf(rem.buttons[0]).address == 0x04);
  CHECK(decodedOf(rem.buttons[0]).command == 0x08);
  CHECK(decodedOf(rem.buttons[0]).bits == 32);
  CHECK(decodedOf(rem.buttons[2]).command == 0x02);  // Vol_up
  CHECK(rem.buttons[0].signal.note == "IRDB");
}

TEST_CASE("each supported parsed protocol maps with the field widths confirmed from Flipper's encoders") {
  std::string t = kHeader;
  t += parsedBlock("nec", "NEC", "04 00 00 00", "08 00 00 00");
  t += parsedBlock("necext", "NECext", "34 12 00 00", "08 F7 00 00");
  t += parsedBlock("sam", "Samsung32", "07 00 00 00", "02 00 00 00");
  t += parsedBlock("sirc12", "SIRC", "01 00 00 00", "15 00 00 00");
  t += parsedBlock("sirc15", "SIRC15", "A5 00 00 00", "15 00 00 00");
  t += parsedBlock("sirc20", "SIRC20", "01 0A 00 00", "15 00 00 00");  // address field 0x0A01 = 5-bit 0x01, ext 0x50
  auto r = parseFlipperIr(t);
  REQUIRE(r.ok());
  REQUIRE(r.value().buttons.size() == 6);
  const auto& b = r.value().buttons;
  CHECK(decodedOf(b[1]).protocol == Protocol::Nec);
  CHECK(decodedOf(b[1]).address == 0x1234);
  CHECK(decodedOf(b[1]).command == 0x08);
  CHECK(decodedOf(b[2]).protocol == Protocol::Samsung);
  CHECK(decodedOf(b[2]).bits == 32);
  CHECK(decodedOf(b[3]).protocol == Protocol::Sony);
  CHECK(decodedOf(b[3]).bits == 12);
  CHECK(decodedOf(b[3]).address == 1);
  CHECK(decodedOf(b[3]).command == 0x15);
  CHECK(decodedOf(b[4]).bits == 15);
  CHECK(decodedOf(b[4]).address == 0xA5);
  CHECK(decodedOf(b[5]).bits == 20);
  CHECK(decodedOf(b[5]).address == 0x01);
  CHECK(decodedOf(b[5]).command == (0x15u | (0x50u << 7)));  // extended bits become the upper command bits
}

TEST_CASE("unsupported and invalid signals are counted and never guessed") {
  std::string t = kHeader;
  t += parsedBlock("rc6", "RC6", "00 00 00 00", "0C 00 00 00");
  t += parsedBlock("badnec", "NEC", "04 01 00 00", "08 00 00 00");           // address > 0xFF for plain NEC
  t += parsedBlock("oddext", "NECext", "34 12 00 00", "08 08 00 00");         // command high byte is not the inverse
  t += parsedBlock("ok", "NEC", "01 00 00 00", "02 00 00 00");
  auto r = parseFlipperIr(t);
  REQUIRE(r.ok());
  CHECK(r.value().report.seen == 4);
  CHECK(r.value().report.imported == 1);
  CHECK(r.value().report.unsupported == 2);
  CHECK(r.value().report.invalid == 1);
  CHECK(r.value().report.firstProblem.find("rc6") == 0);
  CHECK(r.value().buttons.size() == 1);
}

TEST_CASE("RAW signals convert, with limits on frequency and length") {
  std::string t = kHeader;
  t += "# \nname: raw1\ntype: raw\nfrequency: 38000\nduty_cycle: 0.330000\ndata: 9000 4500 560 560 560 1690 560\n";
  t += "# \nname: lowfreq\ntype: raw\nfrequency: 1000\nduty_cycle: 0.33\ndata: 9000 4500 560 560\n";
  t += "# \nname: zero\ntype: raw\nfrequency: 38000\nduty_cycle: 0.33\ndata: 9000 0 560\n";
  auto r = parseFlipperIr(t);
  REQUIRE(r.ok());
  CHECK(r.value().report.imported == 1);
  CHECK(r.value().report.invalid == 2);
  const auto& f = r.value().buttons.at(0).signal.initial.at(0);
  CHECK(f.kind == Frame::Kind::Raw);
  CHECK(f.raw.carrierHz == 38000);
  CHECK(f.raw.durationsUs.size() == 7);
}

TEST_CASE("capacity limits drop extra signals and say so") {
  std::string t = kHeader;
  for (int i = 0; i < 5; ++i) t += parsedBlock("b" + std::to_string(i), "NEC", "01 00 00 00", "0" + std::to_string(i) + " 00 00 00");
  auto r = parseFlipperIr(t, 3);
  REQUIRE(r.ok());
  CHECK(r.value().buttons.size() == 3);
  CHECK(r.value().report.overCapacity == 2);
  CHECK(r.value().report.seen == 5);
}

TEST_CASE("long names are cut on a character boundary, empty names are invalid") {
  std::string t = kHeader;
  t += parsedBlock("Una_tecla_con_nombre_larguisimo_para_el_limite", "NEC", "01 00 00 00", "01 00 00 00");
  t += parsedBlock("\xC3\xA1\xC3\xA1\xC3\xA1\xC3\xA1\xC3\xA1\xC3\xA1\xC3\xA1\xC3\xA1\xC3\xA1\xC3\xA1\xC3\xA1\xC3\xA1", "NEC", "01 00 00 00", "02 00 00 00");
  t += parsedBlock("", "NEC", "01 00 00 00", "03 00 00 00");
  auto r = parseFlipperIr(t);
  REQUIRE(r.ok());
  REQUIRE(r.value().buttons.size() == 2);
  CHECK(r.value().buttons[0].name.size() <= limits::kMaxNameBytes);
  CHECK(validateName(r.value().buttons[1].name).ok());  // 12 two-byte letters: cut to 20 bytes = 10 letters, whole
  CHECK(r.value().report.invalid == 1);
}

TEST_CASE("files that are not Flipper IR files are rejected") {
  CHECK_FALSE(parseFlipperIr("hello world\n").ok());
  CHECK_FALSE(parseFlipperIr("").ok());
  auto v2 = parseFlipperIr("Filetype: IR signals file\nVersion: 2\n");
  REQUIRE_FALSE(v2.ok());
  CHECK(v2.status().code == Err::UnsupportedVersion);
  CHECK(parseFlipperIr("Filetype: IR signals file\nVersion: 1\n").ok());  // valid, empty
}
