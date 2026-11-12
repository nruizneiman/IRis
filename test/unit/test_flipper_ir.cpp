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

TEST_CASE("unknown protocol names and invalid values are counted and never guessed") {
  std::string t = kHeader;
  t += parsedBlock("nuevo", "RC7", "00 00 00 00", "0C 00 00 00");           // not a protocol Flipper defines
  t += parsedBlock("badnec", "NEC", "04 01 00 00", "08 00 00 00");           // address > 0xFF for plain NEC
  t += parsedBlock("badrc6", "RC6", "00 01 00 00", "0C 00 00 00");           // address > 0xFF for RC6
  t += parsedBlock("ok", "NEC", "01 00 00 00", "02 00 00 00");
  auto r = parseFlipperIr(t);
  REQUIRE(r.ok());
  CHECK(r.value().report.seen == 4);
  CHECK(r.value().report.imported == 1);
  CHECK(r.value().report.unsupported == 1);
  CHECK(r.value().report.invalid == 2);
  CHECK(r.value().report.firstProblem.find("nuevo") == 0);
  CHECK(r.value().buttons.size() == 1);
}

namespace {

const RawFrame& rawOf(const ImportedButton& b) {
  REQUIRE(b.signal.initial.at(0).kind == Frame::Kind::Raw);
  return b.signal.initial.at(0).raw;
}

// Reads `bits` pulse-distance bits, LSB first, from the spaces that follow the two header entries.
uint64_t spaceBits(const RawFrame& f, unsigned bits, uint16_t oneAbove) {
  uint64_t v = 0;
  for (unsigned i = 0; i < bits; ++i) v |= static_cast<uint64_t>(f.durationsUs.at(3 + 2 * i) > oneAbove ? 1 : 0) << i;
  return v;
}

}  // namespace

TEST_CASE("RC5 and RC5X become decoded RC5 frames") {
  std::string t = kHeader;
  t += parsedBlock("rc5", "RC5", "05 00 00 00", "35 00 00 00");
  t += parsedBlock("rc5x", "RC5X", "05 00 00 00", "12 00 00 00");
  auto r = parseFlipperIr(t);
  REQUIRE(r.ok());
  REQUIRE(r.value().buttons.size() == 2);
  const auto& b = r.value().buttons;
  CHECK(decodedOf(b[0]).protocol == Protocol::Rc5);
  CHECK(decodedOf(b[0]).bits == 12);
  CHECK(decodedOf(b[0]).address == 0x05);
  CHECK(decodedOf(b[0]).command == 0x35);
  CHECK(decodedOf(b[1]).bits == 13);
  CHECK(decodedOf(b[1]).command == 0x52);  // the seventh command bit of RC5X is implied
}

TEST_CASE("protocols without a decoded form are kept as the RAW frame Flipper would send") {
  std::string t = kHeader;
  t += parsedBlock("necext", "NECext", "34 12 00 00", "08 08 00 00");    // command high byte is not the inverse
  t += parsedBlock("nec42", "NEC42", "34 12 00 00", "A5 00 00 00");
  t += parsedBlock("nec42ext", "NEC42ext", "78 56 34 02", "CD AB 00 00");
  t += parsedBlock("kaseikyo", "Kaseikyo", "80 02 20 00", "D0 03 00 00"); // Panasonic TV power: 40 04 01 00 BC BD
  t += parsedBlock("rca", "RCA", "0F 00 00 00", "00 00 00 00");
  t += parsedBlock("pioneer", "Pioneer", "A5 00 00 00", "1C 00 00 00");
  t += parsedBlock("rc6", "RC6", "00 00 00 00", "0C 00 00 00");
  auto r = parseFlipperIr(t);
  REQUIRE(r.ok());
  CHECK(r.value().report.imported == 7);
  CHECK(r.value().report.unsupported == 0);
  CHECK(r.value().report.invalid == 0);
  REQUIRE(r.value().buttons.size() == 7);
  const auto& b = r.value().buttons;

  const RawFrame& necext = rawOf(b[0]);
  CHECK(necext.carrierHz == 38000);
  REQUIRE(necext.durationsUs.size() == 2 + 32 * 2 + 1);
  CHECK(necext.durationsUs[0] == 9000);
  CHECK(necext.durationsUs[1] == 4500);
  CHECK(spaceBits(necext, 32, 1000) == 0x08081234u);

  const RawFrame& nec42 = rawOf(b[1]);
  REQUIRE(nec42.durationsUs.size() == 2 + 42 * 2 + 1);
  const uint64_t v42 = spaceBits(nec42, 42, 1000);
  CHECK((v42 & 0x1FFF) == 0x1234);
  CHECK(((v42 >> 13) & 0x1FFF) == (~0x1234u & 0x1FFF));
  CHECK(((v42 >> 26) & 0xFF) == 0xA5);
  CHECK(((v42 >> 34) & 0xFF) == 0x5A);

  const uint64_t v42ext = spaceBits(rawOf(b[2]), 42, 1000);
  CHECK((v42ext & 0x3FFFFFF) == 0x2345678);
  CHECK((v42ext >> 26) == 0xABCD);

  const RawFrame& kas = rawOf(b[3]);
  REQUIRE(kas.durationsUs.size() == 2 + 48 * 2 + 1);
  CHECK(kas.durationsUs[0] == 3456);
  CHECK(kas.durationsUs[1] == 1728);
  CHECK(spaceBits(kas, 48, 800) == 0xBD3D00802002ull);  // bytes 02 20 80 00 3D BD, first byte first

  std::vector<uint16_t> rca = {4000, 4000};
  for (int i = 0; i < 4; ++i) { rca.push_back(500); rca.push_back(2000); }   // address 1111
  for (int i = 0; i < 12; ++i) { rca.push_back(500); rca.push_back(1000); }  // command 0, inverse address 0000
  for (int i = 0; i < 8; ++i) { rca.push_back(500); rca.push_back(2000); }   // inverse command
  rca.push_back(500);
  CHECK(rawOf(b[4]).durationsUs == rca);

  const RawFrame& pio = rawOf(b[5]);
  CHECK(pio.carrierHz == 40000);
  REQUIRE(pio.durationsUs.size() == 67 + 1 + 67);
  CHECK(pio.durationsUs[0] == 8500);
  CHECK(spaceBits(pio, 32, 1000) == 0xE31C5AA5u);
  CHECK(pio.durationsUs[67] == 26500);  // the gap before the second copy
  CHECK(pio.durationsUs[68] == 8500);

  // Start bit, mode 000, toggle 0 (twice as long), address 00000000, command 00001100: worked out by hand.
  std::vector<uint16_t> rc6 = {2666, 889, 444, 888, 444, 444, 444, 444, 444, 888, 888};
  for (int i = 0; i < 11; ++i) { rc6.push_back(444); rc6.push_back(444); }
  for (uint16_t d : {444, 888, 444, 444, 888, 444, 444, 444}) rc6.push_back(d);
  CHECK(rawOf(b[6]).carrierHz == 36000);
  CHECK(rawOf(b[6]).durationsUs == rc6);
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
