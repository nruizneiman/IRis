# Wokwi project (prepared, NOT yet validated)

Status: the files here were written from Wokwi's public documentation but **have not been opened
in Wokwi** (no Wokwi account/CLI token was used in Stage 3). Part ids and pin names must be
checked the first time you open `diagram.json`:

- `board-ssd1306`, `wokwi-pushbutton`, `wokwi-ir-receiver`, `wokwi-led`: ids and pin names confirmed in the docs
  (`https://docs.wokwi.com/parts/...`).
- `board-esp32-devkit-c-v4` and the `esp:<gpio>` / `esp:GND.1` / `esp:3V3` pin syntax: **not confirmed**
  by the pages consulted. If Wokwi rejects them, pick the ESP32 board from its part list and rewire.
- `wokwi-resistor` pins `1`/`2` are assumed.

## What can and cannot be simulated

| Item | In Wokwi | Caveat |
|---|---|---|
| ESP32 running the firmware | Yes (`.elf`/`.bin` from PlatformIO) | Build first: `pio run`. |
| SSD1306 128x64 over I2C | Yes | SH1106 is **not** listed in the supported parts (only SH1107, 128x128). The real panel may be SH1106, so Wokwi proves the UI flow only. |
| 5 pushbuttons | Yes | Ideal contacts: no real bounce, so software debouncing is not exercised. |
| IR receiver (`wokwi-ir-receiver`) | Part exists; clicking it sends NEC frames | Not a TSOP38238 model: AGC, burst-length limits, sensitivity and noise are not reproduced. |
| IR transmitter | The LED here is only an indicator on the TX GPIO | No 38 kHz IR optics, MOSFET stage, LED current, supply sag or range. |
| Battery, charger, brownout | No | Test on hardware. |

Wokwi never replaces the electrical and optical tests in the physical test plan.

## Use

```powershell
.\.venv\Scripts\pio.exe run -e esp32dev_ssd1306   # builds .pio/build/esp32dev_ssd1306/firmware.{bin,elf}
```

Then open the `wokwi/` folder with the Wokwi for VS Code extension ("Wokwi: Start Simulator"), or
paste `diagram.json` into a new ESP32 project on wokwi.com. The Stage 4 firmware runs the full UI on the simulated
SSD1306 and the five buttons (IR capture/send report "no disponible" until Stage 5; storage is RAM only).
This has not been run in Wokwi yet.
