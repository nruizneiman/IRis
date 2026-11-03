# User interface (Stage 4)

All text shown on the device is Spanish; code and this document are English. The UI lives in
`lib/iris_core/src/ui/` and talks to hardware only through interfaces, so every screen is tested
on the PC with a fake display and fake buttons (`test/integration/test_ui_flows.cpp`).

## Buttons (five keys, D-pad + OK)

| Key | Short press | Long press (0.7 s) | Held |
|---|---|---|---|
| Up / Down | move / change value (acts on touch) | - | auto-repeat after 0.45 s |
| Left | back (in editors: delete / previous digit) | back to the home screen (editors: cancel) | - |
| Right | context menu of the highlighted row (lists); next / append (editors) | - | - |
| OK | choose / confirm / send | - | - |

Why this layout: with five keys a D-pad plus a centre key keeps "Left = back" and "OK = act" the
same on every screen, and leaves Right for per-item actions (edit, delete, learn) instead of
hiding them behind extra keys. Short presses of Left/Right/OK are reported on **release** and a
long press is reported while held, so holding Left to go home never also triggers "back".
Keys held while the device boots are ignored until released.

## Screen grid

The OLED is treated as a 21 x 8 character grid (cells of 6 x 8 px): row 0 title, rows 1-6 content,
row 7 footer with key hints (replaced by a 2 s message when something happens). Text longer than a
row is cut; list rows with a right-aligned value truncate the label first.

Battery: without a measurement circuit (build without `IRIS_BATTERY_RATIO_PERMILLE`, or a pin that reads under
about 0.3 V) the home title bar says `Bat n/d`. With one it shows an **estimated** level, `~77%`, plus a battery
outline with 0-4 bars (4 from 75 %, 3 from 50 %, 2 from 25 %, 1 from 10 %). The estimate comes from a generic
lithium-ion curve (`app/BatteryLevel.h`, anchor points quoted there), not from this cell, and under load it reads
low; the exact voltage and the estimate are on *Herramientas IR > Diagnóstico HW*.

## Map

```
Inicio (battery text, "*" = unsaved changes)
├─ Controles ─ categorías ─ controles ─ botones
│     OK on a button sends its signal; Right opens: Enviar, Aprender señal, Crear manual,
│     Renombrar, Borrar señal, Eliminar botón. Right on a control: rename, move, duplicate, delete. Right on a category: rename, delete.
├─ Aprender señal ─ pick control ─ pick (or create) button ─ wait ─ review ─ Guardar / Probar / Reintentar
├─ Crear control ─ name ─ category (or new) ─ opens the empty control
├─ Herramientas IR
│     Info del receptor · Captura RAW (reproducir / guardar en botón) · Reproducir RAW (asks first)
│     Últimas capturas (8, RAM only) · Diagnóstico HW · Exploración de códigos (+ registro previo)
│     Registro de eventos · Historial arranques
└─ Configuración
      Invertir video · Espera captura (5/10/15/30 s) · Prueba de botones · Info del equipo
      Diagnóstico · Almacenamiento (counts, wipe-all with confirmation)
```

Rules enforced by the UI: every delete asks for confirmation (`OK` = yes, anything else = no);
a long exploration (> 50 codes) asks again and says the emitter cannot know whether the target
reacted; leaving a screen stops its capture or exploration; the capture history is recorded no matter
which screen is visible; display failures never block the logic (the picture is retried every 2 s).

## Limits of the on-device UI

- **RAW signals cannot be typed in on the device** (hundreds of durations). Create them by capturing
  (Captura RAW / Aprender) or, in the future, with an external editing tool. The domain and the
  simulator (`manual-raw`) support them already.
- Text entry is a character picker (slow by design); names are limited to 20 UTF-8 bytes.
- Only one capture and one transmission at a time; receiver 2 is not enabled.
- Protocol-specific repeat frames are not generated yet (Stage 5 adapter).
- Visual tuning (font legibility, title rule position, inversion command) is pending on the real panel.

## Look and text entry (2026-11-01 redesign, simulator only so far)

Requested style: the icon menu of the nRF-Box firmware (MIT, a complete firmware, not a library: only the layout idea was
used, all code and icons here are original).

- **Title bar**: a lit bar with dark text on every screen. The selected list row is a lit bar too (the `>` marker stays in
  the text for tests). Lists longer than six rows show a 2 px scroll bar in the right-most pixel columns.
- **Home**: `IconGridScreen`, up to six 16x16 icons in 3 columns x 2 rows (`ui/Icons.h`, drawn as text and converted to XBM at
  compile time), the selected one framed and its name centred on the bottom row. Left/Right move by one, Up/Down by a
  row of three, OK opens; the grid wraps.
- **Text entry**: `TextInputScreen` is now an on-screen keyboard: 4 rows of 10 keys plus a row *Aa / Esp. / Borrar / Listo*,
  three pages (lower case, upper case, accents and symbols). A new name starts in upper case and each letter returns to
  lower case. Arrows move over the keys, OK presses one, Left long press cancels.
- **Suggested names**: new button, category and control names first show a list of typical names (OK picks one); the last
  entry, *Escribir otro...*, opens the keyboard. Renames start on the keyboard.
- **Not yet verified on the real panel**: the rendering of dark text on lit bars, key spacing (12 px cells), icon legibility
  and the 5x8 font in the key row. The pictures checked so far come from `FakeDisplay::toPbm` (text drawn as dither blocks).
