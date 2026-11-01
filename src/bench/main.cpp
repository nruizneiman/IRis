// Bring-up bench: a serial console to check one hardware block at a time, without the UI, the
// library or storage. Flash it with `pio run -e bench -t upload`, open the monitor at 115200 and type
// `h`. See docs/physical-test-plan.md for what to expect from each command.
//
// Safety: every command that can drive the emitter is bounded (see the limits below) and needs a
// fresh `arm` first; it disarms itself afterwards. The emitter pin is driven low at boot and nothing
// is sent unless you type commands.
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <Wire.h>
#include <esp_system.h>

#include "board_pins.h"

namespace {

constexpr uint16_t kRawBufferEntries = 1025;
constexpr uint32_t kArmWindowMs = 30000;
constexpr int kMaxBursts = 20;            // `t`: at most 20 bursts per command
constexpr uint16_t kBurstMarkUs = 2000;   // each burst = 2 ms of carrier ...
constexpr uint16_t kBurstSpaceUs = 8000;  // ... then 8 ms of silence (20 % envelope duty at most)
constexpr uint32_t kGateHoldMs = 3000;    // `g`: gate high for 3 s, once per arm

IRsend irsend(board::kIrTx1);
IRrecv irrecv(board::kIrRx1, kRawBufferEntries, 15, true);
uint32_t armedAtMs = 0;
bool armed = false;

const uint8_t kButtonPins[5] = {board::kBtnUp, board::kBtnDown, board::kBtnLeft, board::kBtnRight, board::kBtnOk};
const char* const kButtonNames[5] = {"UP", "DOWN", "LEFT", "RIGHT", "OK"};

void help() {
  Serial.println(F("--- IRis bench ---"));
  Serial.println(F("h            this help"));
  Serial.println(F("m            reset reason, heap, pin map"));
  Serial.println(F("i            I2C scan (OLED should answer at 0x3C or 0x3D)"));
  Serial.println(F("b            button levels, printed on every change for 10 s (press each key)"));
  Serial.println(F("l            raw level of the IR receiver pin, edges counted for 5 s (no remote needed)"));
  Serial.println(F("r            IR receive dump for 15 s: protocol, address, command, raw durations"));
  Serial.println(F("a            battery sense pin: millivolts AT THE PIN (multiply by your divider ratio)"));
  Serial.println(F("arm          allow ONE emitter command within 30 s"));
  Serial.println(F("t [n]        n (1..20, default 1) bounded 38 kHz bursts of 2 ms every 10 ms"));
  Serial.println(F("n            one NEC frame, address 0x00 command 0x08"));
  Serial.println(F("x            LOOPBACK: receiver on, send one NEC frame, print what the receiver saw (emitter must face the TSOP)"));
  Serial.println(F("g            gate high for 3 s (to measure DC current/voltage: needs the series resistor!)"));
  Serial.println(F("s            microSD probe: mount, card type and size, list the root folder (read only)"));
  Serial.println(F("sdput <size> <crc32hex> <path>   receive a file from the PC into /irdb/... (used by tools/irdb_upload.py)"));
  Serial.println(F("sdcrc <path> | sdls [folder]     size and CRC32 of a file under /irdb/ | list a folder (max 40 entries)"));
  Serial.println(F("sdformat ERASE-ALL   like `s`, but FORMATS the card if it has no FAT volume (erases it; exact text required)"));
}

void printResetReason() {
  const char* name = "other";
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: name = "power on"; break;
    case ESP_RST_SW: name = "software"; break;
    case ESP_RST_PANIC: name = "PANIC"; break;
    case ESP_RST_INT_WDT: name = "interrupt watchdog"; break;
    case ESP_RST_TASK_WDT: name = "task watchdog"; break;
    case ESP_RST_WDT: name = "watchdog"; break;
    case ESP_RST_BROWNOUT: name = "BROWNOUT"; break;
    case ESP_RST_EXT: name = "EN pin"; break;
    default: break;
  }
  Serial.printf("reset reason: %s | free heap %u | min free heap %u | largest block %u\n", name, ESP.getFreeHeap(),
                ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
  Serial.printf("pins: SDA=%u SCL=%u RX1=%u RX2=%u TX1=%u TX2=%u BTN(up,down,left,right,ok)=%u,%u,%u,%u,%u BAT=%u\n",
                board::kI2cSda, board::kI2cScl, board::kIrRx1, board::kIrRx2, board::kIrTx1, board::kIrTx2,
                board::kBtnUp, board::kBtnDown, board::kBtnLeft, board::kBtnRight, board::kBtnOk, board::kBatterySense);
}

void scanI2c() {
  Wire.begin(board::kI2cSda, board::kI2cScl);
  Wire.setClock(100000);  // slow and forgiving for a first look
  int found = 0;
  for (uint8_t a = 0x08; a < 0x78; ++a) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  device at 0x%02X\n", a);
      ++found;
    }
  }
  if (!found) Serial.println(F("  nothing answered: check VCC/GND/SDA/SCL, pull-ups, and that no pin is swapped"));
  Serial.println(F("  (the address does not tell SH1106 from SSD1306: use the display_probe environment)"));
}

void watchButtons() {
  for (uint8_t pin : kButtonPins) pinMode(pin, INPUT_PULLUP);
  Serial.println(F("watching buttons for 10 s (1 = released, 0 = pressed)"));
  bool last[5];
  for (int i = 0; i < 5; ++i) last[i] = digitalRead(kButtonPins[i]);
  const uint32_t end = millis() + 10000;
  while (millis() < end) {
    for (int i = 0; i < 5; ++i) {
      const bool now = digitalRead(kButtonPins[i]);
      if (now != last[i]) {
        Serial.printf("%6lu ms  %-5s -> %d\n", static_cast<unsigned long>(millis()), kButtonNames[i], now);
        last[i] = now;
      }
    }
    delay(1);
  }
  Serial.println(F("done. A key that never changed is open/miswired; a key stuck at 0 is shorted or on a strapping pin."));
}

void watchReceiverLevel() {
  pinMode(board::kIrRx1, INPUT);
  Serial.printf("receiver pin %u: idle level should be 1 (the TSOP output idles high)\n", board::kIrRx1);
  uint32_t edges = 0, lowSamples = 0, samples = 0;
  bool last = digitalRead(board::kIrRx1);
  const uint32_t end = millis() + 5000;
  while (millis() < end) {
    const bool now = digitalRead(board::kIrRx1);
    if (now != last) ++edges;
    last = now;
    lowSamples += now ? 0 : 1;
    ++samples;
    delayMicroseconds(50);
  }
  Serial.printf("edges in 5 s: %lu, time low: %.1f %%  (with no remote in view both should be ~0)\n",
                static_cast<unsigned long>(edges), samples ? 100.0 * lowSamples / samples : 0.0);
}

void receiveDump() {
  Serial.printf("receiving on pin %u for 15 s - point a remote at the receiver and press keys\n", board::kIrRx1);
  irrecv.enableIRIn();
  const uint32_t end = millis() + 15000;
  int captures = 0;
  decode_results results;
  while (millis() < end) {
    if (irrecv.decode(&results)) {
      ++captures;
      Serial.printf("#%d type=%s bits=%u addr=0x%lX cmd=0x%lX repeat=%d overflow=%d\n", captures,
                    typeToString(results.decode_type).c_str(), results.bits, static_cast<unsigned long>(results.address),
                    static_cast<unsigned long>(results.command), results.repeat, results.overflow);
      Serial.print(F("   raw(us, mark first): "));
      for (uint16_t i = 1; i < results.rawlen && i < 121; ++i) {
        Serial.print(static_cast<uint32_t>(results.rawbuf[i]) * kRawTick);
        Serial.print(i + 1 < results.rawlen ? "," : "");
      }
      if (results.rawlen > 121) Serial.print(F(" ...(truncated in print)"));
      Serial.println();
    }
    delay(2);
  }
  irrecv.disableIRIn();
  Serial.printf("done: %d captures\n", captures);
}

void readBattery() {
  analogReadResolution(12);
  uint32_t sum = 0;
  for (int i = 0; i < 16; ++i) {
    sum += analogReadMilliVolts(board::kBatterySense);
    delay(2);
  }
  Serial.printf("pin %u: %lu mV at the pin (ESP32 ADC is non-linear: compare with a multimeter, do not trust blindly)\n",
                board::kBatterySense, static_cast<unsigned long>(sum / 16));
}

bool checkArmed() {
  if (!armed || millis() - armedAtMs > kArmWindowMs) {
    armed = false;
    Serial.println(F("not armed: type `arm` first (it allows one emitter command for 30 s)"));
    return false;
  }
  armed = false;  // one command per arm
  return true;
}

void burst(int n) {
  if (n < 1) n = 1;
  if (n > kMaxBursts) n = kMaxBursts;
  const uint16_t pattern[2] = {kBurstMarkUs, kBurstSpaceUs};
  for (int i = 0; i < n; ++i) irsend.sendRaw(pattern, 2, 38000);
  digitalWrite(board::kIrTx1, LOW);
  Serial.printf("sent %d burst(s) of %u us at 38 kHz (software PWM, library default duty)\n", n, kBurstMarkUs);
}

// End-to-end check of the whole chain: gate -> MOSFET -> LED -> air -> TSOP -> decoder. The receiver ISR
// keeps capturing while the (blocking) send runs, so one board can test itself.
void loopback() {
  decode_results results;
  irrecv.enableIRIn();
  delay(60);
  irsend.send(NEC, irsend.encodeNEC(0x00, 0x08), 32, 0);
  digitalWrite(board::kIrTx1, LOW);
  const uint32_t end = millis() + 300;
  bool seen = false;
  while (millis() < end && !seen) {
    if (irrecv.decode(&results)) {
      seen = true;
      Serial.printf("receiver saw: type=%s bits=%u addr=0x%lX cmd=0x%lX raw entries=%u overflow=%d\n",
                    typeToString(results.decode_type).c_str(), results.bits, static_cast<unsigned long>(results.address),
                    static_cast<unsigned long>(results.command), results.rawlen > 0 ? results.rawlen - 1 : 0,
                    results.overflow);
      const bool ok = results.decode_type == NEC && results.address == 0x00 && results.command == 0x08;
      Serial.println(ok ? F("LOOPBACK OK: the emitter chain produced a frame the receiver decodes as sent")
                        : F("LOOPBACK PARTIAL: something arrived but it is not the frame that was sent"));
      if (!ok) {
        // A NEC frame sent as 9000/4500 us header, then 562/562 (bit 0) or 562/1688 (bit 1), ending with a 562 us mark.
        Serial.print(F("   raw(us, mark first): "));
        for (uint16_t i = 1; i < results.rawlen && i < 121; ++i) {
          Serial.print(static_cast<uint32_t>(results.rawbuf[i]) * kRawTick);
          Serial.print(i + 1 < results.rawlen ? "," : "");
        }
        Serial.println();
      }
    }
    delay(2);
  }
  if (!seen) Serial.println(F("LOOPBACK FAILED: the receiver saw nothing (aim, distance, LED current, MOSFET, wiring?)"));
  irrecv.disableIRIn();
}

void gateHold() {
  Serial.printf("gate HIGH for %lu ms: measure the LED current / MOSFET drop now\n", static_cast<unsigned long>(kGateHoldMs));
  digitalWrite(board::kIrTx1, HIGH);
  delay(kGateHoldMs);
  digitalWrite(board::kIrTx1, LOW);
  Serial.println(F("gate LOW again"));
}

// ---- SD file transfer from the PC over the serial port (used to load Flipper-IRDB `.ir` files) ----
// Only paths under /irdb/ are accepted, a file is written to `<path>.tmp` and renamed after its CRC32 matches.
bool sdMounted = false;

bool sdMount() {
  if (sdMounted) return true;
  static SPIClass spi(VSPI);
  spi.begin(board::kSdSck, board::kSdMiso, board::kSdMosi, board::kSdCs);
  if (!SD.begin(board::kSdCs, spi, 4000000)) {
    Serial.println(F("ERR sd mount failed"));
    return false;
  }
  sdMounted = true;
  return true;
}

uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return crc;
}

bool pathAllowed(const String& p) {
  if (!p.startsWith("/irdb/") || p.length() > 96 || p.endsWith("/") || p.indexOf("..") >= 0 || p.indexOf("//") >= 0)
    return false;
  for (size_t i = 0; i < p.length(); ++i) {
    const char c = p[i];
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == '_' ||
                    c == '-' || c == '.' || c == ' ' || c == '(' || c == ')';
    if (!ok) return false;
  }
  return true;
}

void sdMakeParents(const String& path) {
  for (int i = 1; i < static_cast<int>(path.length()); ++i) {
    if (path[i] == '/') {
      const String dir = path.substring(0, i);
      if (!SD.exists(dir)) SD.mkdir(dir);
    }
  }
}

// args: "<size> <crc32 hex> <path>"
void sdPut(const String& args) {
  const int s1 = args.indexOf(' ');
  const int s2 = s1 < 0 ? -1 : args.indexOf(' ', s1 + 1);
  if (s1 < 0 || s2 < 0) { Serial.println(F("ERR usage: sdput <size> <crc32hex> <path>")); return; }
  const unsigned long size = args.substring(0, s1).toInt();
  const uint32_t want = static_cast<uint32_t>(strtoul(args.substring(s1 + 1, s2).c_str(), nullptr, 16));
  const String path = args.substring(s2 + 1);
  if (size == 0 || size > 65536) { Serial.println(F("ERR size must be 1..65536")); return; }
  if (!pathAllowed(path)) { Serial.println(F("ERR path must be /irdb/... with letters, digits, _ - . ( ) and spaces")); return; }
  if (!sdMount()) return;
  sdMakeParents(path);
  const String tmp = path + ".tmp";
  File f = SD.open(tmp, FILE_WRITE);
  if (!f) { Serial.println(F("ERR cannot create the file")); return; }
  Serial.println(F("READY"));
  uint8_t buf[256];
  uint32_t crc = 0xFFFFFFFFu;
  unsigned long left = size;
  while (left > 0) {
    const size_t n = left < sizeof(buf) ? left : sizeof(buf);
    size_t got = 0;
    uint32_t last = millis();
    while (got < n && millis() - last < 5000) {
      if (Serial.available()) {
        got += Serial.readBytes(buf + got, n - got);
        last = millis();
      }
    }
    if (got < n) {
      f.close();
      SD.remove(tmp);
      Serial.println(F("ERR timeout waiting for data"));
      return;
    }
    if (f.write(buf, n) != n) {
      f.close();
      SD.remove(tmp);
      Serial.println(F("ERR write failed (card full or protected?)"));
      return;
    }
    crc = crc32Update(crc, buf, n);
    left -= n;
    Serial.write('A');  // chunk acknowledged: the PC may send the next one
  }
  f.close();
  crc ^= 0xFFFFFFFFu;
  if (crc != want) {
    SD.remove(tmp);
    Serial.printf("\nERR crc mismatch: received %08lX, expected %08lX\n", static_cast<unsigned long>(crc),
                  static_cast<unsigned long>(want));
    return;
  }
  if (SD.exists(path)) SD.remove(path);
  if (!SD.rename(tmp, path)) { Serial.println(F("\nERR rename failed")); return; }
  Serial.printf("\nOK size=%lu crc=%08lX path=%s\n", size, static_cast<unsigned long>(crc), path.c_str());
}

void sdCrc(const String& path) {
  if (!pathAllowed(path)) { Serial.println(F("ERR path must be /irdb/...")); return; }
  if (!sdMount()) return;
  File f = SD.open(path, FILE_READ);
  if (!f || f.isDirectory()) { Serial.println(F("ERR no such file")); return; }
  uint8_t buf[256];
  uint32_t crc = 0xFFFFFFFFu;
  unsigned long total = 0;
  while (true) {
    const int n = f.read(buf, sizeof(buf));
    if (n <= 0) break;
    crc = crc32Update(crc, buf, static_cast<size_t>(n));
    total += static_cast<unsigned long>(n);
  }
  f.close();
  Serial.printf("CRC size=%lu crc=%08lX path=%s\n", total, static_cast<unsigned long>(crc ^ 0xFFFFFFFFu), path.c_str());
}

void sdList(String dir) {
  if (dir.isEmpty()) dir = "/";
  if (!sdMount()) return;
  File d = SD.open(dir);
  if (!d || !d.isDirectory()) { Serial.println(F("ERR not a folder")); return; }
  int shown = 0;
  for (File e = d.openNextFile(); e && shown < 40; e = d.openNextFile(), ++shown)
    Serial.printf("  %s%s  %lu bytes\n", e.name(), e.isDirectory() ? "/" : "", static_cast<unsigned long>(e.size()));
  if (shown == 0) Serial.println(F("  (empty)"));
  if (shown == 40) Serial.println(F("  ... (listing stops at 40 entries)"));
}

// Read-only probe of the SD module: nothing is written, the card is unmounted afterwards.
// With allowFormat the library formats the card ONLY when the mount fails with "no valid FAT volume" (FatFs error 13);
// a card that already carries a FAT volume is never formatted by this path.
void sdProbe(bool allowFormat = false) {
  if (sdMounted) {
    SD.end();
    sdMounted = false;
  }
  static SPIClass spi(VSPI);
  spi.begin(board::kSdSck, board::kSdMiso, board::kSdMosi, board::kSdCs);
  Serial.printf("SD: SCK=%u MISO=%u MOSI=%u CS=%u, 4 MHz%s\n", board::kSdSck, board::kSdMiso, board::kSdMosi,
                board::kSdCs, allowFormat ? ", FORMAT ALLOWED if there is no FAT volume" : "");
  if (!SD.begin(board::kSdCs, spi, 4000000, "/sd", 5, allowFormat)) {
    Serial.println(F("SD: mount failed (no card, wiring, power, or the card is not FAT16/FAT32)"));
    return;
  }
  const char* type = "unknown";
  switch (SD.cardType()) {
    case CARD_MMC: type = "MMC"; break;
    case CARD_SD: type = "SD (SDSC)"; break;
    case CARD_SDHC: type = "SDHC/SDXC"; break;
    default: break;
  }
  Serial.printf("SD: card type %s, size %llu MB\n", type, SD.cardSize() / (1024ULL * 1024ULL));
  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    Serial.println(F("SD: cannot open the root folder"));
    SD.end();
    return;
  }
  int shown = 0;
  for (File f = root.openNextFile(); f && shown < 20; f = root.openNextFile(), ++shown) {
    Serial.printf("  %s%s  %lu bytes\n", f.name(), f.isDirectory() ? "/" : "", static_cast<unsigned long>(f.size()));
  }
  if (shown == 0) Serial.println(F("  (root folder is empty)"));
  if (shown == 20) Serial.println(F("  ... (listing stops at 20 entries)"));
  root.close();
  SD.end();
}

}  // namespace

void setup() {
  pinMode(board::kIrTx1, OUTPUT);  // first: emitter gate low
  digitalWrite(board::kIrTx1, LOW);
  Serial.begin(115200);
  delay(300);
  irsend.begin();
  digitalWrite(board::kIrTx1, LOW);
  printResetReason();
  help();
}

void loop() {
  if (!Serial.available()) {
    delay(5);
    return;
  }
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.isEmpty()) return;
  const char c = line[0];
  if (line == "arm") {
    armed = true;
    armedAtMs = millis();
    Serial.println(F("armed for 30 s: the next emitter command (t, n or g) will run once"));
  } else if (c == 'h') {
    help();
  } else if (c == 'm') {
    printResetReason();
  } else if (c == 'i') {
    scanI2c();
  } else if (c == 'b') {
    watchButtons();
  } else if (c == 'l') {
    watchReceiverLevel();
  } else if (c == 'r') {
    receiveDump();
  } else if (c == 'a') {
    readBattery();
  } else if (c == 't') {
    if (checkArmed()) burst(line.length() > 1 ? line.substring(1).toInt() : 1);
  } else if (c == 'n') {
    if (checkArmed()) {
      irsend.send(NEC, irsend.encodeNEC(0x00, 0x08), 32, 0);
      digitalWrite(board::kIrTx1, LOW);
      Serial.println(F("sent one NEC frame (address 0x00, command 0x08)"));
    }
  } else if (c == 'x') {
    if (checkArmed()) loopback();
  } else if (c == 'g') {
    if (checkArmed()) gateHold();
  } else if (line.startsWith("sdput ")) {
    sdPut(line.substring(6));
  } else if (line.startsWith("sdcrc ")) {
    sdCrc(line.substring(6));
  } else if (line == "sdls" || line.startsWith("sdls ")) {
    sdList(line.length() > 5 ? line.substring(5) : String());
  } else if (line == "sdformat ERASE-ALL") {
    sdProbe(true);
  } else if (c == 's') {
    sdProbe();
  } else {
    Serial.println(F("unknown command, type h"));
  }
}
