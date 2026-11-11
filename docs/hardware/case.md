# Case

A two-part printed case with five printed keys. It is modeled in Autodesk Fusion ("IRis Mando v2") and published on
[MakerWorld](https://makerworld.com/en/models/3592563-iris-universal-ir-remote-case-esp32-oled).

| File in [`case/`](../../case) | What it is |
|---|---|
| `IRIS_Case.3mf` | Print-ready export |
| `IRis Mando v2.f3d` | Fusion model, with the parameters named in this document |
| `IRis Mando v2.step` | The same geometry in a neutral CAD format |
| `IRis Mando v2 v24.png` | Preview picture |

The author's unit is assembled in a print of this case. The outside is **58.5 x 194.5 x 33.2 mm**.

## How the size is decided

The outer size is derived from the dimensions of the modules, so a builder with slightly different parts changes a
parameter and the case follows. The module parameters are in millimetres and are named `<module>_largo`,
`<module>_ancho`, `<module>_alto` (length, width, height):

| Prefix | Module | Values in the model (mm, length x width x height) |
|---|---|---|
| `bat_` | 18650 holder with the cell | 76 x 21.2 x 19.7 |
| `esp_` | ESP32 dev board on female headers | 52.2 x 28.5 x 19 |
| `oled_` | 1.3" OLED with its socket | 34.6 x 35.7 x 18 |
| `xl_` | XL6009 boost converter | 43.3 x 20.8 x 12.8 |
| `car_` | USB-C charger module | 27.8 x 17.4 x 2.6 (4.1 with the USB-C connector) |
| `sd_` | microSD module with its header | 51.5 x 23.8 x 8 |
| `plb_` | Button board | 23.7 x 23.7 x 8 |
| `plq_` | [Component board](component-board.md) | 45.9 x 28.8 x 10 |
| `sw_` | Rocker switch window | 13.1 long x 8.3 high |

Derived from them: `ancho_int`, `largo_int`, `alto_int` (inside) and `ancho`, `largo`, `alto_total` (outside). The
clearances and the room for wires are parameters too: `holg_int`, `holg_z`, `sep_mod`, `sep_z`, `canal_cables`. The
walls are 2.4 mm (`pared`) and the corners have a 5 mm radius (`r_esq`).

Measure your own modules before printing: boards with the same name differ by a millimetre or two between makers.

## Layout

From the IR end to the other end:

1. The IR LEDs and the receiver, held by a bulkhead with a hole per LED and a slot for the receiver, and a short wall
   between them so that the LEDs do not blind the receiver. The number of LEDs is the parameter `led_n` (2).
2. The OLED, with the microSD module behind it.
3. The button board, with the XL6009 behind it.
4. The battery holder on the left, with the switch window above it, next to the ESP32 column on the right: ESP32,
   charger and component board.

The micro-USB of the ESP32 is at the end opposite the LEDs, off-centre on purpose because the battery and the ESP32
sit side by side. The USB-C of the charger and the microSD slot are on the right wall, and the switch window is on the
left wall.

## Holding the modules

- **Screw posts for M3 screws** threading into the plastic. One parameter, `tornillo_pil` (2.5 mm pilot hole), drives
  the pilots of the OLED, the ESP32, the XL6009, the component board and the button board. The posts are 5 mm wide.
- **Holes of the boards.** The holes of the OLED, the ESP32 and the XL6009 are 3 mm: ream them to 3.2 mm. Drill the
  holes of the component board and the button board at 3.2 mm.
- **Screw lengths.** ESP32 and button board M3 x 6, component board and XL6009 M3 x 5, OLED M3 x 4. With M3 x 5 screws
  on the OLED, put one printed washer (1 mm) under each head; four washers are in the model as separate bodies.
- **microSD module.** Its holes are 2 mm, too small for M3, so two ribs hanging from the front wall hold it down. The
  posts for M2 screws (1.6 mm pilot) are still in the model for anyone who has them.
- **Battery holder and charger.** They have no usable holes: two hold-down ribs each hang from the front wall and stop
  0.2 mm above the module. The charger sits on two rails.
- **Pockets.** Walls of 1.2 mm around each module on the floor, and frames for the OLED and the button board on the
  front wall. The pockets of the component board and the charger have small ribs that press 0.1 mm on the board edge
  (`crush_i`), so the board stays in place before it is screwed. If a board does not go in, lower `crush_i`.
- Each group of posts or ribs is its own Fusion feature, so a builder can suppress what does not apply.

## Print settings

PLA, 0.4 mm nozzle.

- Layer height 0.2 mm. The walls (2.4 mm) are 6 perimeters and the ribs (1.2 mm) are 3. The engraving depths and the
  height of the split between base and cover are multiples of 0.2 mm (`altura_capa`).
- No supports. Print the **base with its floor on the bed**, the **cover with the front face on the bed** and the
  **keys with the flange on the bed**. The window cuts in the side walls bridge 14 mm at most.
- 15 to 20 % infill is enough. Print the washers flat at 100 % infill.
- Do not scale the print: the parameters already include the clearances (0.2 mm between base and cover, 0.4 mm in the
  module pockets, 0.25 mm around the keys). If a part is tight on your printer, change the clearance parameter.
- The largest part is 194.5 x 58.5 mm: it fits a 220 x 220 mm bed.
- The helper body `Envolvente exterior` is hidden in the model. Do not export it.

## Openings

- The end of the ESP32 board, the charger board and the microSD board each sit in a recess cut into the inner face of
  the wall, so only a thin skin is left in front of the connector: 1.2 mm at the ESP32 (`esp_skin`) and 0.8 mm at the
  others (`conn_skin`). Each opening has 0.3 mm of clearance around the connector (`conn_c`).
- The micro-USB opening is a rectangle with chamfered lower corners, with an entry slot down to the seam so that the
  cover can come down over the connector.
- The USB-C opening is a rounded slot for a shell of 8.7 x 3 mm.
- The card slot is 15 x 2.8 mm. The card stays slightly inside the wall: push it with a fingernail and it springs out.
- The base and the cover snap together with a lip (`prof_leng` 5 mm) and small bumps (`bump_d` 0.6 mm, `holgura`
  0.2 mm). If the cover will not close, raise `holgura` or lower `bump_d`.

## Keys

- The four direction keys are rounded triangles pointing away from the centre, each with an engraved triangle. The OK
  key is a rounded square turned 45 degrees with the letters OK engraved. The shape of each key stops it from turning.
- The keys print with the flange on the bed and the stem up, so the engraving (0.6 mm deep, `key_prof`) is on the
  last layers.
- The switch pitch is 7.3 mm (`pad_pitch`), measured on the button board. The hole in the cover is the key plus
  0.25 mm (`btn_clr`), and the flange that retains the key is the key plus 0.6 mm (`fl_ov`).
- The OK lettering is 2.2 mm high (`ok_h`), about one extrusion wide: small for a 0.4 mm nozzle. Print one key alone
  first, which takes about ten minutes.

## Engravings

- The `IRIS` text on the front face is engraved 0.8 mm deep, in a single colour.
- Labels inside the pockets (BATTERY, ESP32, CHARGER, PCB, XL6009, MICROSD, PWR, IR, OLED, BUTTONS) are 3 mm letters
  engraved 0.4 mm. They may print faint; raise `serig_h` or `serig_prof` if they do not read. They are not needed for
  the remote to work.

## Values to check against your parts

These parameters were estimated instead of measured, or depend on the exact module: `tsop_ancho`, `tsop_alto`,
`tsop_largo` (receiver slot), `esp_usb_z`, `car_usb_z`, `esp_pcb_z` (connector heights), `sd_post_h`, `xl_post_h`,
`sd_slot_z`, `sd_slot_h` (card slot), `oled_vidrio` (display window) and `plb_sw_h` (switch height).

## Revision notes

How the model changed between prints, kept because the reasons are useful to anyone who modifies it. Each note was
written on its day: a value quoted here may have changed later (the current ones are above and in the Fusion file),
and "not tested in print" describes that moment.

### Changes after the first full print (2026-11-05)

- OLED pocket 1 mm longer (`oled_largo` 34.6); ESP32 posts 4 mm (`esp_poste_d`); XL6009 pocket follows the module (`xl_largo`
  43.3); LED partition thicker (`led_sep` 2); receiver slot from the measured TSOP (`tsop_ancho` 5, `tsop_alto` 7,
  `tsop_largo` 4.8).
- microSD: `sd_ancho` 22.8, header-end posts moved toward the slot (`sd_hx_adj`), slot raised and taller (`sd_slot_z` 4,
  `sd_slot_h` 2.8). These two values are estimates: check them with the card.
- ESP32 end: the skin in front of the board is `esp_skin` (1.2 mm). With 0.8 mm the rounded corner of the cover broke
  through and left a slit next to the micro-USB opening. The micro-USB opening is a rectangle with chamfered lower corners
  (`usb_mc_chh`, `usb_mc_chv`).
- Snap fit: `holgura` 0.2 and `bump_d` 0.6 (was 0.25 and 0.5). If the cover will not close, go back.
- Component board wires: two notches (`cable_w` 6 mm wide, floor `cable_z0` above the base floor) in the short walls of
  its pocket, one toward the XL6009 (`cable_x_sup`) and one toward the charger (`cable_x_inf`), positioned relative to the
  pocket and clear of the XL6009 post and the charger ribs.
- The four direction keys carry an engraved triangle (`tri_g_b` x `tri_g_h`, `key_prof` deep), like OK.
- Button board cut down to 23.7 x 23.7 mm (`plb_largo`, `plb_ancho`) so that the five switches sit at its centre. The
  frame around it follows `plb_zona` (the larger of the board and the envelope of the key flanges, 29.7 mm), so the board is
  centred in a frame that is bigger than itself and is held only by its four M2 screws (holes 3 mm from its edges).

### Change after the second print (2026-11-05)

- The lip of the cover (`prof_leng` 5 mm, z 11.6 to 16.6) ran across the whole ESP32 end and hit the end of the board
  and its pin, so the cover did not close with the ESP32 screwed down. A notch now removes the lip across the board width
  (`esp_ancho` plus `esp_leng_h` 0.4 mm each side) and from the recess floor to the inner face of the wall plus
  `esp_leng_y` 0.2 mm; the side lip toward the right wall stays. Feature "Corte Escotadura de la lengueta para el ESP32".
  Not tested in print: it also weakens the snap at that end a little.

- The micro-USB opening in the cover was closed below, so the strip of wall under it hit the top of the connector while the
  cover came down. It now has an entry slot of the same width running from the seam to the opening
  (`usb_mc_ranura_ext` 0.2 mm overlap; feature "Corte Ranura de entrada micro-USB", cover only). The first version stopped
  at the bottom of the hexagon, which left two small wall legs at the chamfered corners; the slot now rises to the top of
  the chamfers (`usb_mc_chv`), so the opening is a full-width channel from the seam to the top. Not tested in print.

- At the IR end the lip had a small island between the LED window and the receiver window (about 2.5 mm wide, 0.75 mm
  thick) that broke off easily and held nothing. It is cut away (`ir_leng_h` 0.1 mm margin; feature "Corte Quitar lengueta
  fina lado IR", cover only); its limits follow `ir_w`, `tsop_x` and `tsop_ancho`.

### Holding the boards in the base

- The pocket walls of the component board and of the charger used to stop 0.2 mm above the underside of the board, so they
  held nothing. They now rise to the top of the board plus 0.4 mm (`enc_top_plq`, `enc_top_car`) and each long wall has
  two small ribs (`crush_w` 0.8 mm wide) that press 0.1 mm (`crush_i`) on the board edge, so the board is held by friction
  and is not loose when the base is alone. There is no flexing hook: neighbouring pockets share their walls. Not tested in
  print: if a board does not go in, lower `crush_i`; if it still moves, raise it (0.2 mm at most).
- The charger board measures 27.8 x 17.4 mm and is 2.6 mm high without the USB-C connector (4.1 mm with it).

### M3 screws instead of M2 (2026-11-06)

M2 screws were hard to find, so the screw posts are now sized for **M3**. One parameter, `tornillo_pil` (2.5 mm pilot
hole for M3 in PLA), drives the pilots of the OLED, ESP32, XL6009, component board and button board (`oled_pil`,
`esp_pil`, `xl_pil`, `plq_pil`, `plb_pil`). The posts keep their diameters (`poste_d` 5 mm; `esp_poste_d` 4 mm, so the
wall around the ESP32 pilot is only 0.75 mm, below the 1.2 mm rule: if a post splits, lower `tornillo_pil` to 2.4 or widen
`esp_poste_d` if the sockets allow).

- **Boards:** the module holes of the OLED, ESP32 and XL6009 are 3 mm: ream them to 3.2 mm. The component and button
  boards are drilled by the builder: drill them to 3.2 mm. A 3.2 mm hole on a 2.54 mm grid just touches the neighbouring
  pads (the keep-out cells of the [component board](component-board.md) still apply).
- **Pilot depths in the model:** ESP32 15 mm, button board 7 mm (from the front wall), component board 3.8 mm, XL6009
  3.3 mm, OLED 2.3 mm (from the front wall). Screw lengths that fit: ESP32 and button board M3 x 6, component board and
  XL6009 M3 x 5, OLED M3 x 4. A 5.5 mm pan head is wider than the M2 head: check it clears the pins next to the ESP32
  holes.
- **microSD:** its holes are 2 mm with the centres 1.2 mm from the edge, so M3 cannot be used. It is held by two
  hold-down ribs hanging from the front wall (feature "Costillas microSD": `hd_sd_w` 4.5 mm, `hd_sd_dx` 4.5 mm from the
  card-side posts, `hd_sd_ov` 1 mm over the board edge, stopping `hd_luz` 0.2 mm above the board), one on each long edge,
  clear of the OLED frame and of the holes. The microSD posts and their 1.6 mm pilots stay in the model, so anyone with
  M2 screws can still screw the board down (the ribs sit 20.5 to 25 mm from the origin, the screw heads at 27.25).
  Not tested in print; the ribs assume the card socket does not reach the long edges of the board.

### OLED washers for M3 x 5 (2026-11-06)

Only M3 x 5 and M3 x 6 screws were available, but the OLED posts were drawn for M3 x 4 (pilot 2.3 mm). A printed washer
under each OLED screw head makes up the difference: `arandela_h` = `arandela_tornillo_disp` (5) - `arandela_tornillo_dis` (4)
= 1 mm, outer diameter `arandela_od` 6.4 mm, hole `arandela_id` 3.4 mm. Four washers sit in the model, off to the side
(x from `arandela_x`, pitch `arandela_paso`), as separate bodies "Arandela OLED 1..4"; a case export must skip them or
send them to their own folder. Print them flat, 4 pieces, 100 % infill.

### Thicker ESP32 posts (2026-11-06)

With the 2.5 mm pilot the 4 mm ESP32 posts left a 0.75 mm wall, which risks splitting when the M3 screw cuts its thread. The
parameter `esp_poste_d` is now `2 * 1.25 mm + tornillo_pil` (5.0 mm now), so the wall stays at 1.25 mm whatever the pilot is.
Check when assembling that the wider posts do not touch the header pins under the ESP32.

### End lip removed at the ESP32 end (2026-11-06)

The cover lip along the ESP32 end wall (the strip that was left on both sides of the earlier notch) still stopped the cover
from closing, so it is cut away completely: cut "Corte Quitar lengueta extremo ESP32", from the lip bottom to the seam, across the
whole inner width less `esp_leng_borde` (0.5 mm) of side wall. The side lips and rounded corners stay, so the cover keeps its
centring along the long sides. The ESP32 is held by its screws only.

The ledge that was still there (2026-11-06, second try): the ESP32 wall window of the cover left a 0.4 mm-thick inner strip under it
(z 17.0 to 17.4), right under the board. The window now goes down to `z_union - 0.2 mm`, so that strip is gone and only the outer
1.2 mm skin remains over the ESP32 width.

### microSD module 1 mm wider (2026-11-06)

The microSD board measured 23.7 mm on its short side, not 22.8, so it did not fit. `sd_ancho` is now 23.8 mm (23.7 + 0.1 clearance). The
hole pitch across (`sd_hy`, now 21.4 mm), the pocket and the hold-down ribs follow the parameter; the edge on the OLED side stays and the
other edge moves 1 mm toward the IR end.
