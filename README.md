# IRis

A programmable universal infrared remote you can build yourself: an ESP32, a small OLED, five buttons, a microSD
card and one 18650 cell in a 3D-printed case.

IRis learns the buttons of the remotes you already have, sends them back, and can use the thousands of remotes of the
[Flipper-IRDB](https://github.com/Lucaslhm/Flipper-IRDB) collection straight from the card.

> **Status:** the author's unit is assembled in its printed case, runs from the battery and works. It was
> validated with a multimeter and by using it, not with lab instruments, and the build guides are still closer to an
> engineering notebook than to a step-by-step manual. See [What works and what does not](#what-works-and-what-does-not).

## What it does

- **Learn and send.** Point a remote at the receiver, press a key, save it as a button. Buttons live in remotes, and
  remotes in categories.
- **Protocols.** NEC, Samsung, Sony (SIRC), RC5 and JVC are decoded and stored as protocol, address and command.
  Anything else is kept and replayed as a raw capture.
- **Remotes from the microSD card.** Copy Flipper-IRDB `.ir` files to `/irdb` and use them directly from the card, or
  import one remote or a whole folder into the device. Your own remotes can be exported back as `.ir` files.
- **Listen.** Keeps capturing until you stop it, then lets you replay, inspect or save each capture.
- **Be Gone.** Sends the power code of every remote in a category or in a card folder, each distinct code once.
- **Loop and crazy modes.** Repeat a sequence of buttons, or send random ones, with a chosen pause.
- **Manual codes and exploration.** Type a protocol, address and command by hand, or sweep a bounded range of codes.
- **English and Spanish** interface, an on-screen keyboard with suggested names, a battery estimate, and diagnostics
  screens (state, event log, boot history, button test).

Nothing is transmitted at boot or without a key press, sweeps are bounded and ask for confirmation, and every delete
asks first. Use it on equipment that is yours.

## Hardware

| Part | Notes |
|---|---|
| ESP32-WROOM-32 dev board, 30 pins | The pin map is in [`include/board_pins.h`](include/board_pins.h) |
| OLED 1.3" 128x64, I2C | SSD1306 or SH1106 (there is a firmware build for each) |
| TSOP38238 IR receiver | 38 kHz |
| 2 IR LEDs + NPN transistor (2N2222) | Each LED with its own resistor |
| 5 tactile switches, 6 x 6 mm | Up, down, left, right, OK |
| microSD module, SPI | Optional: needed for the card features only |
| 18650 cell and holder, USB-C charger module with protection, XL6009 boost converter, power switch | The boost converter feeds 5 V to the board |
| Two 10 kΩ resistors | Optional: battery level on screen |
| Perfboard, pin headers, M3 screws | |

The printable case is on [**MakerWorld**](https://makerworld.com/en/models/3592563-iris-universal-ir-remote-case-esp32-oled#profileId-4115008). How it is designed and how to print it:
[`docs/case-print.md`](docs/case-print.md).

Wiring and assembly:

| Document | What it is |
|---|---|
| [`docs/bom.md`](docs/bom.md) | Bill of materials: parts, quantities, screws and printed pieces |
| [`docs/wiring.md`](docs/wiring.md) | The design guide: every block, the safety rules and the reasoning |
| [`docs/breadboard-wiring.md`](docs/breadboard-wiring.md) | The circuit exactly as it was wired and worked on the bench |
| [`docs/plaquita.md`](docs/plaquita.md), [`docs/assembly.md`](docs/assembly.md) | The soldered component board and the connectors |
| [`docs/hardware.md`](docs/hardware.md) | What was measured, how to identify your OLED, first checks on a new board |
| [`docs/physical-test-plan.md`](docs/physical-test-plan.md) | Bring-up of a new unit, block by block |

A lithium cell stores a lot of energy: read the safety rules at the top of `docs/wiring.md` before connecting one.

## Build and flash

You need Python 3 and a USB cable. The firmware is a [PlatformIO](https://platformio.org/) project; the versions it was
built with are in [`docs/dependencies.md`](docs/dependencies.md).

The commands below are for Windows PowerShell, which is what the project is developed on. On Linux or macOS the same
`pio` commands work with `.venv/bin/pio`.

```powershell
python -m venv .venv
.\.venv\Scripts\pip install platformio

# Not sure which controller your OLED has? This shows a test picture with both drivers.
.\.venv\Scripts\pio.exe run -e display_probe -t upload

# Build and flash: esp32dev_ssd1306 for an SSD1306 panel, esp32dev for an SH1106 one
.\.venv\Scripts\pio.exe run -e esp32dev_ssd1306 -t upload
```

If the upload does not start, hold the BOOT button of the board while it connects.

The `esp32dev_ssd1306` build expects the battery divider on GPIO33. Without it the screen shows `Bat n/a`; to leave it
out of the build, remove the `IRIS_BATTERY_RATIO_PERMILLE` line in [`platformio.ini`](platformio.ini).

There is also a serial console that tests one hardware block at a time (I2C, buttons, receiver, emitter, card, ADC):
`pio run -e bench -t upload`, then `pio device monitor`. Its commands are in the test plan.

## Using it

Five keys: **Up / Down** move, **OK** chooses or sends, **Left** goes back (hold it to return to the home screen) and
**Right** opens the menu of the highlighted row.

| Home icon | What is there |
|---|---|
| Remotes | Your categories, remotes and buttons, and the card's folders marked `SD`. Everything you can do to a remote (rename, duplicate, loop, crazy, export, import) is on its Right-key menu |
| Learn signal | Pick a remote and a button, then press the key of the original remote |
| Listen | Capture several signals in a row and decide later what to do with each |
| Be Gone | Power codes of a whole category or card folder |
| IR tools | Capture history, code exploration, export everything to the card |
| Settings | Language, display, capture timeout, storage, diagnostics |

To use Flipper-IRDB, copy its folders into `/irdb` on a FAT-formatted card. Exports are written to `/iris/export`.

The full screen map and the rules of the interface are in [`docs/ui.md`](docs/ui.md).

## What works and what does not

On the author's unit: display, buttons, learning and sending, the card (browse, import, export), the modes, saving to
flash and running from the battery all work. With two LEDs at about 34 mA each a TV responds from almost 2 m.

Known limits:

- **Range is modest.** The LED current was calculated, not measured; more range needs more LEDs or more current, and
  measuring first.
- **No air conditioners.** Protocols that send the whole state (most A/C units) are not decoded; some may work as raw
  captures.
- **One frame per button.** The repeat pattern of a held key is not learned.
- **Not while charging.** The charger module has no power path: charge with the device switched off.
- **Battery percentage is an estimate** from a generic lithium-ion curve.
- A second receiver and a second emitter channel have pins reserved but are not implemented.

The complete list is in [`docs/risks.md`](docs/risks.md), and the state of each part in
[`docs/status.md`](docs/status.md).

## Developing

The core is plain C++17 with no Arduino headers, so the logic and every screen run on a PC against fake hardware: about
210 test cases drive the real UI through simulated key presses, and the IR codec is cross-checked against the real
IRremoteESP8266 sources.

| Path | Purpose |
|---|---|
| `lib/iris_core/src` | Hardware-independent core: `domain/`, `app/`, `persistence/`, `import/`, `ui/`, `hardware/` (interfaces only) |
| `lib/iris_ir/src` | Mapping between the domain model and IRremoteESP8266 |
| `lib/iris_testkit/src` | Host-only fakes and the `UiRig` fake device. Never linked into the firmware |
| `src/` | ESP32 firmware: `main.cpp`, `infrastructure/esp32/` adapters, `probe/` (display probe), `bench/` (serial console) |
| `include/board_pins.h` | The only place with GPIO numbers |
| `test/` | Unit tests, scripted UI scenarios, the codec cross-check, and frozen data files |
| `simulator/` | Console simulator over the real services and UI |
| `tools/` | Test runner and scripts that copy Flipper-IRDB files to the card over serial |
| `third_party/` | Vendored single-header dependencies of the host build (doctest, ArduinoJson) |

Host toolchain: a C++17 compiler, CMake and Ninja (on Windows: WinLibs MinGW-w64 GCC through winget).

```powershell
./tools/test.ps1                                       # builds and runs all PC tests
./build/iris_sim.exe                                   # interactive simulator, type 'help'
./build/iris_sim.exe simulator/scripts/ui_demo.txt     # a scripted session on the real screens
```

The codec cross-check needs the library sources that PlatformIO downloads, so run one firmware build first.

Rules of the code base:

- Code, comments and documents are in English. Screen text is written in Spanish and translated when drawn
  (`ui/I18n.cpp`); a new screen needs its strings added to that catalog.
- GPIO numbers exist only in `include/board_pins.h`.
- Nothing transmits at boot. Limits are centralised in `lib/iris_core/src/domain/Limits.h` and in `TxSafety`
  (`src/infrastructure/esp32/IrTransmitterAdapter.h`).
- A passing PC test never means "works on hardware".
- Do not run `pio pkg install --library` here: it rewrites `platformio.ini` and drops its comments.

More detail: [`docs/persistence.md`](docs/persistence.md) (storage format and recovery),
[`docs/ir-notes.md`](docs/ir-notes.md) (the IR layer and its limits).

## Licence and credits

IRis is free software under the [GNU General Public License v3.0](LICENSE).

It builds on [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) (LGPL-2.1),
[U8g2](https://github.com/olikraus/u8g2) (BSD 2-Clause), [ArduinoJson](https://arduinojson.org/) (MIT) and
[doctest](https://github.com/doctest/doctest) (MIT). The `.ir` file format and the remote database come from the
Flipper Zero community; [Flipper-IRDB](https://github.com/Lucaslhm/Flipper-IRDB) is CC0 and is not bundled, apart from
one file kept as a test fixture. The icon-menu layout was inspired by the nRF-Box firmware; all code and icons here
are original.
