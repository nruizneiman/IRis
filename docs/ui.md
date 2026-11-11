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
about 0.3 V) the home title bar says `Bat n/d`. With one it shows an **estimated** level, `77%`, plus a battery
outline with 0-4 bars (4 from 75 %, 3 from 50 %, 2 from 25 %, 1 from 10 %). The estimate comes from a generic
lithium-ion curve (`app/BatteryLevel.h`, anchor points quoted there, with 4.10 V taken as 100 %), not from this cell, and under load it reads
low; the exact voltage and the estimate (marked `~`) are on *Configuración > Diagnóstico > Estado*. The shown number is
corrected for the voltage lost between the cell and the divider (a fixed 0.11 V measured on this unit, `kBatteryLoadDropMv`) and smoothed (`BatteryEstimator`: one sample every 0.5 s, exponential average, and it moves only when the new value differs
by 2 points or more) so it does not flicker with ADC noise or transmission dips; after power-on it takes a few
seconds to settle.

## Map

One rule decides where things are: **each action has a single place**. What is done to one control, button or category
is on the Right-key menu of its row in *Controles*; the home screen only holds what does not start from a control.

```
Inicio (six icons on one page; battery text, "*" = unsaved changes)
├─ Controles ─ categorías ─ controles ─ botones          (and the card's folders, marked SD)
│     OK on a button sends its signal.
│     Right on a button:   Enviar, Aprender señal, Crear manual, Renombrar, Borrar señal, Eliminar botón
│     Right on a control:  Renombrar, Cambiar categoría, Duplicar, Exportar a SD, Modo loop, Modo loco, Eliminar
│     Right on a category: Renombrar, Exportar a SD, Eliminar
│     Last row of each list: + Nueva categoría / + Nuevo control / + Nuevo botón
│     A card folder: » Importar carpeta, subfolders, controls. A card control opens read-only (OK sends);
│     Right on it: Importar, Modo loop, Modo loco.
├─ Aprender señal ─ pick control ─ pick (or create) button ─ wait ─ review ─ Guardar / Probar envío / Reintentar
├─ Escuchar ─ keeps capturing until OK; each capture: Reproducir, Guardar en botón, Detalle, Eliminar
├─ Be Gone ─ Apagar / Encender, pause ─ category or card folder ─ confirm ─ run
├─ Herramientas IR
│     Últimas capturas (8, RAM only; OK replays the RAW after asking) · Exploración (+ registro previo)
│     Exportar todo a SD
└─ Configuración
      Idioma · Invertir video · Espera captura (5/10/15/30 s) · Almacenamiento (counts, wipe-all with confirmation)
      Diagnóstico: Estado · Info del equipo · Prueba de botones · Registro de eventos · Historial arranques
```

Removed in the 2026-11 clean-up because another entry already did the same: the home icons *Crear control* (use
*+ Nuevo control* inside a category), *Modo loop* / *Modo loco* (menu of a control) and *Importar* (the card's folders in
*Controles*); *Captura RAW* (*Escuchar* captures, replays and saves), *Reproducir RAW* (first row of *Últimas capturas*),
*Info del receptor*, the second *Diagnóstico* entry, and the setting *SD en Controles* (the card's folders are always
listed when a card is in).

Rules enforced by the UI: every delete asks for confirmation (`OK` = yes, anything else = no);
a long exploration (> 50 codes) asks again and says the emitter cannot know whether the target
reacted; leaving a screen stops its capture or exploration; the capture history is recorded no matter
which screen is visible; display failures never block the logic (the picture is retried every 2 s).

## Limits of the on-device UI

- **RAW signals cannot be typed in on the device** (hundreds of durations). Create them by capturing
  (Escuchar / Aprender) or, in the future, with an external editing tool. The domain and the
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

## Language

English is the default; Spanish is selectable in *Settings > Language* (the first row of the settings) and is saved with the settings (`lang=` line).
The screens are written with Spanish text and translated when drawn (`ui/I18n.h`, catalog Spanish -> English; unknown text
is shown as written). Consequence: names the user typed or imported are never translated, except that a name equal to a
catalog entry (for example a category called "Controles") is shown translated. The scripted tests run in Spanish
(`UiRig`), and `test_language.cpp` walks the main screens in English and fails if Spanish text is left on them. A new
screen needs its strings added to the catalog.


## Modes and the card (2026-11)

The home screen is an icon grid of six entries on one page (Up/Down switch between the two rows, Left/Right walk
through all of them): Controles, Aprender señal, Escuchar, Be Gone, Herramientas IR, Configuración. The grid still pages
by itself (six per page, one dot per page) if more entries are ever added.

**Controls from the card, next to the saved ones.** A card control is read from its `.ir` file when it is opened and
never stored (`app/RemoteView.h`: a saved control and a card control are both a `RemoteView`, and the modes only see that).
- *Controles* lists the saved categories and, marked `SD`, what is in `/irdb` on the card: its top-level folders and any
  control lying directly in it. Without a card they are not listed. A card control opens read-only: OK sends a button,
  Right opens a menu (Importar, Modo loop, Modo loco) that also says how many buttons of the file could not be used.
- Importing is done from there, so a control can be tried before it is imported. *» Importar carpeta*, the first row of
  every card folder, turns every `.ir` file directly in it into a saved control, in a new category named after the folder
  or an existing one, a file per tick so Left can stop it.

**Modes** (all send through `TransmitService`, so the emitter limits apply; none sends before its screen is opened and the
user chooses to start; Left stops, leaving the screen stops, and the pause between sends is never below 100 ms):
- *Modo loop* (menu of a control): mark the buttons (the mark shows the order), choose the pause (0.2 s to 60 s), start; the sequence repeats until
  stopped.
- *Modo loco* (menu of a control): random buttons of one control, sometimes the same one two or three times in a row; the pause varies between
  0.5x and 1.5x the chosen one.
- *Be Gone*: choose Apagar or Encender, then a saved category or a card folder (subfolders included), confirm, and it sends the
  power button of each control (an explicit off / on button when there is one, else the toggle). A code already sent in the run
  is not sent twice, which keeps a run over a whole IRDB folder short. The emitter gives no feedback, so it cannot say which
  code worked.
- *Escuchar*: keeps the receiver armed, restarting it after every capture, until OK stops it (up to 40 captures / 16000 RAW
  entries); then a list where each capture can be replayed, saved into a button of a saved control, inspected or deleted. A
  burst that arrives in the instant between one capture and the next arming can be missed.

**Limits.** A saved control holds up to 256 buttons (it was 40), the library 64 controls and 1200 buttons in total (they live
in RAM); a card control up to 512 buttons, from a file of at most 64 KB. These are RAM budgets, not a promise of "unlimited".

**Export.** *Herramientas IR > Exportar todo a SD*, and *Exportar a SD* in the menu of a control or a category, write Flipper `.ir`
files to `/iris/export/<category>/<control>.ir` (`import/FlipperIrWriter.h`, the mirror of the importer: NEC / NECext,
Samsung32, SIRC / SIRC15 / SIRC20 and raw; other decoded protocols and empty buttons are counted and left out). Names keep
ASCII only (any other character becomes `_`). A file is written under a temporary name and renamed. Re-importing them goes
through *Controles* once the folder is copied under `/irdb`.
