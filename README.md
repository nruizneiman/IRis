# IRis - programmable universal IR remote (ESP32)

Status: **software complete for the first hardware test; nothing has been run on the real board yet.**
The firmware builds, the PC tests pass, and the bring-up tools and guides for the physical validation exist.
Read `docs/status.md` for what is functional, cross-checked, only compiled, or pending.

## Where to start

| You want to | Read / run |
|---|---|
| Wire the hardware | `docs/wiring.md` |
| Validate it block by block | `docs/physical-test-plan.md` + `docs/test-log-template.md` |
| Identify the OLED controller | `docs/hardware.md`, environment `display_probe` |
| Test one block with a serial console | environment `bench` (commands in the test plan) |
| Understand the UI | `docs/ui.md` |
| Understand storage and recovery | `docs/persistence.md` |
| Understand the IR layer and its limits | `docs/ir-notes.md` |
| See open risks and unknowns | `docs/risks.md` |

## Layout

| Path | Purpose |
|---|---|
| `lib/iris_core/src` | Hardware-independent C++17 core: `domain/`, `app/`, `persistence/`, `ui/`, `hardware/` (interfaces only). No Arduino headers. |
| `lib/iris_ir/src` | Mapping between the domain model and IRremoteESP8266 (ESP32, and the PC with the library in unit-test mode). |
| `lib/iris_testkit/src` | Host-only fakes, synthetic fixtures and the `UiRig` fake device. Never linked into the firmware. |
| `src/` | ESP32 firmware: `main.cpp`, `infrastructure/esp32/` adapters, `probe/` (display probe), `bench/` (serial bring-up console). |
| `include/board_pins.h` | The single GPIO map (proposed, unverified on your board). |
| `test/unit`, `test/integration`, `test/hostlib` | Unit tests, scripted UI/simulator scenarios, and the cross-check against the real IRremoteESP8266 code. |
| `test/fixtures` | Frozen data files (the format-1 library) used to keep old formats readable. |
| `simulator/` | Console simulator over the real services and UI; `scripts/` has sample sessions. |
| `wokwi/` | Wokwi project, prepared but never opened. |
| `third_party/` | Vendored single-header deps for the host build (doctest, ArduinoJson). |

## Commands (Windows / PowerShell)

Toolchain: MinGW-w64 GCC (WinLibs via winget), CMake, Ninja, PlatformIO in `.venv`.

```powershell
# All PC tests (unit + integration + IR cross-check). No ESP32 needed.
./tools/test.ps1
ctest --test-dir build                                 # after one build: ir_crosscheck, all, unit, integration

# Console simulator (all hardware simulated)
./build/iris_sim.exe                                   # interactive, type 'help'
./build/iris_sim.exe simulator/scripts/demo.txt        # services
./build/iris_sim.exe simulator/scripts/ui_demo.txt     # real screens and keys

# Firmware builds (esp32dev = SH1106 driver, esp32dev_ssd1306 = SSD1306 driver)
.\.venv\Scripts\pio.exe run -e esp32dev
.\.venv\Scripts\pio.exe run -e esp32dev_ssd1306

# Bring-up (board connected): display probe, then the serial bench console
.\.venv\Scripts\pio.exe run -e display_probe -t upload
.\.venv\Scripts\pio.exe run -e bench -t upload
.\.venv\Scripts\pio.exe device monitor

# Flash the real firmware
.\.venv\Scripts\pio.exe run -e esp32dev -t upload
```

Open a new terminal after the winget install so `g++`, `cmake` and `ninja` are on PATH, or use `tools/test.ps1`,
which refreshes PATH itself.

## Rules of the project

- Code, comments and docs are in English; the device UI is in Spanish.
- GPIO numbers exist only in `include/board_pins.h`.
- Nothing transmits at boot. Limits are centralised in `lib/iris_core/src/domain/Limits.h` and in `TxSafety`
  (`IrTransmitterAdapter.h`) and are provisional until the hardware is measured.
- A passing PC test never means "works on hardware". Hardware validation is `docs/physical-test-plan.md`.
- Do not run `pio pkg install --library` in this project: it rewrites `platformio.ini` and drops its comments.
