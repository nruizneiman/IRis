// Display probe: finds out which driver your 1.3" OLED needs. Flash it (env display_probe), then watch
// the panel and the serial monitor (115200). It scans I2C and alternates the SAME test picture
// between the SH1106 driver and the SSD1306 driver every 6 seconds.
//
// The correct driver shows a clean border touching all four edges, two diagonals and sharp text.
// The wrong one typically shows the picture shifted by about 2 px, a column of noise at one edge,
// or a wrapped line. Report what you see: the I2C scan only gives the address, not the controller.
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

#include "board_pins.h"

U8G2_SH1106_128X64_NONAME_F_HW_I2C sh1106(U8G2_R0, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C ssd1306(U8G2_R0, U8X8_PIN_NONE);

static void scan() {
  Serial.println("[probe] I2C scan:");
  int found = 0;
  for (uint8_t a = 0x08; a < 0x78; ++a) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  device at 0x%02X\n", a);
      ++found;
    }
  }
  if (!found) Serial.println("  nothing found: check SDA/SCL/VCC/GND wiring");
}

static void draw(U8G2& u, const char* label, uint8_t addr) {
  u.setI2CAddress(static_cast<uint8_t>(addr << 1));
  u.begin();
  u.clearBuffer();
  u.drawFrame(0, 0, 128, 64);
  u.drawLine(0, 0, 127, 63);
  u.drawLine(0, 63, 127, 0);
  for (int x = 4; x < 124; x += 8) u.drawPixel(x, 2);  // tick marks along the top edge
  u.setFont(u8g2_font_6x10_tf);
  u.drawStr(10, 24, label);
  u.drawStr(10, 36, "Borde completo?");
  u.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Wire.begin(board::kI2cSda, board::kI2cScl);
  Wire.setClock(400000);
  scan();
}

void loop() {
  uint8_t addr = 0;
  for (uint8_t c : {0x3C, 0x3D}) {
    Wire.beginTransmission(c);
    if (Wire.endTransmission() == 0) {
      addr = c;
      break;
    }
  }
  if (!addr) {
    Serial.println("[probe] no display at 0x3C/0x3D");
    delay(2000);
    return;
  }
  Serial.println("[probe] showing: SH1106 driver");
  draw(sh1106, "Driver: SH1106", addr);
  delay(6000);
  Serial.println("[probe] showing: SSD1306 driver");
  draw(ssd1306, "Driver: SSD1306", addr);
  delay(6000);
}
