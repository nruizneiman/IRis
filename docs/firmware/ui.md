# User interface

How the device is operated, where each function is, and the rules the screens follow.

The device shows English (the default) or Spanish (*Settings > Language*). This document uses the English names. In the
code the screens are written in Spanish and translated when drawn; see [Language](#language).

The UI lives in `lib/iris_core/src/ui/` and talks to the hardware only through interfaces, so every screen is tested
on the PC with a fake display and fake buttons (`test/integration/`).

## Keys

Five keys: a D-pad and OK.

| Key | Short press | Long press (0.7 s) | Held |
|---|---|---|---|
| Up / Down | Move, or change a value. Acts on touch | - | Repeats after 0.45 s |
| Left | Back. In editors: delete, or previous digit | Back to the home screen. In editors: cancel | - |
| Right | Menu of the highlighted row. In editors: next, or append | - | - |
| OK | Choose, confirm or send | - | - |

Why this layout: with five keys, "Left = back" and "OK = act" stay the same on every screen, and Right is free for the
actions of one item (rename, delete, learn) instead of hiding them behind extra keys.

Short presses of Left, Right and OK are reported on **release**, and a long press while the key is held, so holding
Left to go home never also triggers "back". Keys held while the device boots are ignored until released.

## Map

One rule decides where things are: **each action has a single place**. What is done to one remote, button or category
is on the Right-key menu of its row in *Remotes*. The home screen only holds what does not start from a remote.

```
Home (six icons on one page; the title bar shows the battery, and "*" when there are unsaved changes)
├─ Remotes ─ categories ─ remotes ─ buttons              (and the folders of the card, marked SD)
│     OK on a button sends its signal.
│     Right on a button:   Send, Learn signal, Create manual, Rename, Delete signal, Delete button
│     Right on a remote:   Rename, Change category, Duplicate, Export to SD, Loop mode, Crazy mode, Delete
│     Right on a category: Rename, Export to SD, Delete
│     Last row of each list: + New category / + New remote / + New button
│     A card folder: » Import folder, subfolders, remotes. A card remote opens read-only (OK sends);
│     Right on it: Import, Loop mode, Crazy mode.
├─ Learn signal ─ pick a remote ─ pick or create a button ─ wait ─ review ─ Save / Test send / Retry
├─ Listen ─ keeps capturing until OK; each capture: Play, Save to button, Details, Delete
├─ Be Gone ─ Power off / Power on, pause ─ a category or a card folder ─ confirm ─ run
├─ IR tools
│     Latest captures (8, kept in RAM only; OK replays the capture after asking)
│     Exploration (and the log of the previous run)
│     Export all to SD
└─ Settings
      Language · Invert video · Capture wait (5 / 10 / 15 / 30 s) · Storage (counts, erase all with confirmation)
      Diagnostics: Status · Device info · Button test · Event log · Boot history
```

Rules the UI enforces:

- Every delete asks for confirmation: OK is yes, any other key is no.
- Nothing is transmitted until the user starts it, and leaving a screen stops its capture, mode or exploration.
- A long exploration (more than 50 codes) asks again and says that the emitter cannot know whether the target reacted.
- The capture history is recorded whichever screen is visible.
- A display failure never blocks the logic: the picture is retried every 2 s.

## Remotes from the card

A card remote is read from its `.ir` file when it is opened and is never stored. In the code a saved remote and a card
remote are both a `RemoteView` (`app/RemoteView.h`), and the modes only see that.

- *Remotes* lists the saved categories and, marked `SD`, what is in `/irdb` on the card: its top-level folders and any
  remote lying directly in it. Without a card they are not listed.
- A card remote opens read-only: OK sends a button. Its Right-key menu (Import, Loop mode, Crazy mode) also says how
  many buttons of the file could not be used.
- Importing is done from there, so a remote can be tried before it is imported.
- *» Import folder*, the first row of every card folder, turns every `.ir` file directly in it into a saved remote, in
  a new category named after the folder or in an existing one. It imports one file per tick, so Left can stop it.

**Export.** *IR tools > Export all to SD*, and *Export to SD* in the menu of a remote or a category, write Flipper `.ir`
files to `/iris/export/<category>/<remote>.ir` (`import/FlipperIrWriter.h`, the mirror of the importer).

- Exported: NEC / NECext, Samsung32, SIRC / SIRC15 / SIRC20 and raw signals. Other decoded protocols and empty buttons
  are counted and left out.
- Names keep ASCII only; any other character becomes `_`.
- A file is written under a temporary name and then renamed.
- To import them again, copy the folder under `/irdb`.

## Modes

All of them send through `TransmitService`, so the emitter limits apply. None sends before its screen is opened and
the user chooses to start. Left stops, leaving the screen stops, and the pause between sends is never below 100 ms.

- **Loop mode** (menu of a remote). Mark the buttons, and the mark shows the order; choose the pause (0.2 s to 60 s)
  and start. The sequence repeats until stopped.
- **Crazy mode** (menu of a remote). Random buttons of one remote, sometimes the same one two or three times in a row.
  The pause varies between 0.5 and 1.5 times the chosen one.
- **Be Gone.** Choose Power off or Power on, then a saved category or a card folder (subfolders included), and confirm.
  It sends the power button of each remote: an explicit off or on button when there is one, otherwise the toggle. A
  code already sent in the run is not sent twice, which keeps a run over a whole IRDB folder short. The emitter gets
  no feedback, so it cannot say which code worked.
- **Listen.** Keeps the receiver armed, restarting it after every capture, until OK stops it (up to 40 captures or
  16000 raw entries). Then each capture can be replayed, saved into a button of a saved remote, inspected or deleted.
  A burst that arrives in the instant between one capture and the next arming can be missed.

## Limits

- A saved remote holds up to 256 buttons. The library holds 64 remotes, 16 categories and 1200 buttons in total: they
  live in RAM.
- A card remote holds up to 512 buttons, from a file of 64 KB at most.
- Names are limited to 20 UTF-8 bytes.
- **Raw signals cannot be typed in on the device** (hundreds of durations). They come from a capture.
- One capture and one transmission at a time.
- Exploration is bounded: 1000 codes at most and 250 ms or more between them.

The limits are RAM budgets, centralised in `lib/iris_core/src/domain/Limits.h`.

## Screen

The OLED is treated as a grid of 21 x 8 characters (cells of 6 x 8 px): row 0 is the title, rows 1 to 6 the content,
and row 7 the footer with key hints, replaced for 2 s by a message when something happens. Text longer than a row is
cut; in a list row with a right-aligned value, the label is cut first.

- **Title bar.** A lit bar with dark text on every screen. The selected row of a list is a lit bar too. Lists longer
  than six rows show a 2 px scroll bar at the right edge.
- **Home.** `IconGridScreen`: six 16 x 16 icons in 3 columns and 2 rows (`ui/Icons.h`), the selected one framed and
  its name centred on the bottom row. Left and Right move by one, Up and Down by a row, OK opens. The grid wraps, and
  it would page by itself, six per page, if more entries were added.
- **Text entry.** `TextInputScreen` is an on-screen keyboard: 4 rows of 10 keys plus a row *Aa / Spc / Del / Done*, in
  three pages (lower case, upper case, accents and symbols). A new name starts in upper case and each letter returns
  to lower case. The arrows move over the keys, OK presses one, and a long press of Left cancels.
- **Suggested names.** A new button, category or remote first shows a list of typical names; OK picks one. The last
  entry, *Type another...*, opens the keyboard. Renames start on the keyboard.

The screens are in daily use on the author's SSD1306 panel. The accented letters and `ñ ¡ ¿` of the Spanish text were
not checked glyph by glyph.

The icon-menu layout was inspired by the nRF-Box firmware; only the idea was used, and all code and icons are original.

## Battery

- Without a measurement circuit (a build without `IRIS_BATTERY_RATIO_PERMILLE`, or a pin that reads under about 0.3 V)
  the title bar of the home screen says `Bat n/a`.
- With one it shows an **estimated** level, such as `77%`, and a battery outline with 0 to 4 bars (4 from 75 %, 3 from
  50 %, 2 from 25 %, 1 from 10 %).
- The estimate comes from a generic lithium-ion curve (`app/BatteryLevel.h`, with 4.10 V taken as 100 %), not from
  this cell, and under load it reads low. The exact voltage and the estimate, marked `~`, are on
  *Settings > Diagnostics > Status*.
- The number is corrected for the voltage lost between the cell and the divider: a fixed 0.11 V measured on the
  author's unit (`kBatteryLoadDropMv`).
- It is smoothed so that it does not flicker with ADC noise or transmission dips (`BatteryEstimator`: one sample every
  0.5 s, an exponential average, and it moves only when the new value differs by 2 points or more). After power-on it
  takes a few seconds to settle.

## Language

English is the default. Spanish is selected in *Settings > Language*, the first row of the settings, and saved with
them.

- The screens are written with Spanish text and translated when drawn (`ui/I18n.cpp`, a Spanish to English catalog).
  Text that is not in the catalog is shown as written.
- Names that the user typed or imported are not translated. The exception: a name equal to a catalog entry, such as a
  category called "Controles", is shown translated.
- A new screen needs its strings added to the catalog. The scripted tests run in Spanish (`UiRig`), and
  `test_language.cpp` walks the main screens in English and fails if Spanish text is left on them.
