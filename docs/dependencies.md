# Dependencies

## Verified in Stage 2 (resolved and used in a successful build)

| Item | Version | Where | Evidence |
|---|---|---|---|
| PlatformIO Core | 6.2.0 | `.venv` | `pio --version` |
| Platform `espressif32` | 6.12.0 (pinned) | `platformio.ini` | firmware built |
| Arduino-ESP32 core | 2.0.17 | installed `framework-arduinoespressif32` reports `3.20017.241212` | package.json (PlatformIO encodes 2.0.17 as 3.20017.x) |
| Xtensa GCC | 8.4.0 (2021r2-patch5) | PlatformIO package | package.json |
| ArduinoJson | 7.4.2 (pinned in `lib_deps` and vendored for the host build) | firmware + `third_party/` | firmware built; host tests pass |
| U8g2 | 2.36.18 (pinned in `lib_deps`) | firmware | all three firmware environments built; drawing API names (`drawGlyph`, `setFontPosTop`, `sendF`, both `U8G2_SH1106/SSD1306_128X64_NONAME_F_HW_I2C` classes) compiled. Behaviour on the panel NOT verified. |
| IRremoteESP8266 | 2.9.0 (pinned in `lib_deps`) | firmware + PC cross-check | API read from the installed sources; all environments build; host cross-check passes (`docs/ir-notes.md`). Behaviour on the ESP32 NOT verified. |
| LittleFS | part of Arduino-ESP32 2.0.17 | firmware | compiles; mount/rename behaviour NOT verified on hardware |
| doctest | 2.4.11 (vendored) | `third_party/doctest` | host tests pass |
| Host compiler | GCC 16.2.0 (WinLibs MinGW-w64 UCRT) | winget | `g++ --version` |

## Not yet verified / chosen

| Item | State |
|---|---|
| IRremoteESP8266 with core 3.x | Unknown; the project stays on core 2.0.17 |
| U8g2 font `u8g2_font_5x8_tf` | Compiles; that it renders every Spanish glyph used (n-tilde, accents, inverted ! ?) must be checked on the panel |
| Wokwi | Documentation lists SSD1306 and a 38 kHz IR receiver; SH1106 unsupported; project never opened |
| Library sources on the PC | `test/hostlib` reads them from `.pio/libdeps/esp32dev/IRremoteESP8266` (created by `pio run`); not vendored because of the LGPL licence and size |

## Flipper-IRDB (data, not linked code)

- Source: https://github.com/Lucaslhm/Flipper-IRDB. Licence: **CC0 1.0 Universal** (LICENSE file and README read 2026-11-01).
  The README download links point at a different owner (logickworkshop/Flipper-IRDB); whether that is a move or a fork reference is unconfirmed.
- Used as: `.ir` text files copied to the SD card and converted by `lib/iris_core/src/import/FlipperIr.*`. Nothing from the
  repository is bundled in the firmware. One real file is kept as a test fixture (`test/fixtures/irdb_LG.ir`, see `irdb_README.txt`).
- Field meaning for NEC / NECext / Samsung32 / SIRC was confirmed by reading Flipper's encoders (flipperzero-firmware, lib/infrared/encoder_decoder). Other protocols are skipped and counted, not guessed.
