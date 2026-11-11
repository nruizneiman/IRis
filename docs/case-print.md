# Case: parametric model and print notes

The case is modeled in Autodesk Fusion ("IRis Mando v2", project "IRis Control Remoto"). The files are in
[`case/`](../case): `IRis Mando v2.f3d` (Fusion, with the parameters described below), `IRis Mando v2.step` (neutral
CAD format), `IRIS_Case.3mf` (print-ready export) and a preview image. The case is also on
[MakerWorld](https://makerworld.com/en/models/3592563-iris-universal-ir-remote-case-esp32-oled).

The notes below were written while the case was being printed and revised (the first print was on 2026-11-05).
Everything that says "estimated" or "not tested in print" was not measured.

## How the size is decided

The outer size is derived from the dimensions of the modules, so a builder with slightly different parts changes a
parameter and the case follows. Module parameters are in millimetres, named `<module>_largo`, `<module>_ancho`,
`<module>_alto` (length, width, height):

| Prefix | Module | Values used (mm, length x width x height) |
|---|---|---|
| `bat_` | 18650 holder with the cell | 76 x 21.2 x 19.7 |
| `esp_` | ESP32 dev board with female headers | 52.2 x 28.5 x 18.6 |
| `oled_` | 1.3" OLED with its socket | 33.6 x 35.7 x 18 |
| `xl_` | XL6009 boost converter | 43.3 x 20.8 x 12.8 |
| `car_` | USB-C charger module | 26.8 x 17.3 x 4.1 |
| `sd_` | microSD module with its socket | 50.4 x 23.7 x 6.5 |
| `plb_` | button board (soldered by hand) | 29.5 x 30 x 8 |
| `plq_` | component board (estimated) | 30 x 27 x 10 |
| `sw_` | rocker switch window | 8.3 high x 13.1 long, 17 deep |

Derived: `ancho_int`, `largo_int`, `alto_int` (interior), and `ancho`, `largo`, `alto_total` (exterior). With the values
above the case is about **58.5 x 193.4 x 31.8 mm**. Clearances and room for wires are parameters too: `holg_int`,
`holg_z`, `sep_mod`, `sep_z`, `canal_cables`.

Layout, top (IR end) to bottom: IR LEDs and TSOP (held by a bulkhead with a hole per LED and a slot for the receiver,
and a short shield wall between them), OLED (microSD behind it, at the bottom of the OLED zone), button board (XL6009 right
below the microSD, behind the top of the button zone), then the battery column on the left (switch window above it)
next to the ESP32 column on the right (ESP32, charger, component board, which reaches up to the XL6009). ESP32 micro-USB is at the bottom end, off-centre on purpose (battery and ESP32 sit side by side). Charger USB-C
and the microSD slot are on the right wall, the switch window is on the left wall.

## Holding the modules

- Screw posts with a 1.6 mm pilot hole for M2 screws threading into PLA, M2 x 5 mm being the length the posts are sized
  for (the same screw for every module; the 3 mm holes of the OLED, ESP32 and XL6009 are loose around M2, so a small M2
  washer under the head helps; drill the holes of your own boards to 2.2 mm): OLED (4, holes 3 mm at 30 x 28), ESP32 (4, 3 mm at 22.8 x
  46.3), microSD (4, 2 mm at 37.2 x 19.2), XL6009 (2, 3 mm, opposite corners), component board and button board (4 each,
  3 mm in from the edges: the builder drills them).
- Pocket walls (1.2 mm = 3 perimeters, 4 mm high, 0.4 mm clearance) around each module on the floor, frames for the OLED
  and the button board on the front wall.
- Battery holder and charger have no usable holes: two hold-down ribs each hang from the front wall and stop 0.2 mm above
  the module; the charger sits on two 3 mm rails.
- Each post group and rib group is its own Fusion feature, so a builder can suppress what does not apply.

## Print settings (PLA, 0.4 mm nozzle)

- Layer height 0.2 mm. `pared` (2.4 mm) is 6 perimeters, `rib_t` (1.2 mm) is 3, engraving depths (0.4 and 0.8 mm) and
  the base / cover split heights are multiples of 0.2 mm (`altura_capa`).
- No supports needed as modeled. Print the **base with its floor on the bed** and the **cover with the front face on
  the bed**. The window cuts in the side walls bridge at most 14 mm.
- 15 to 20 % infill is enough. Keep 6 perimeters for the walls (starting suggestion, not tested).
- The model keeps the wall around every screw pilot hole at 1.2 mm or more (3 perimeters), as a starting point.
- Do not scale the print: the parameters already include clearances (0.25 mm lap, 0.4 mm module pockets, 0.5 mm key
  holes). If a part is tight on your printer, change the clearance parameter, not the scale.
- Largest part is 193.4 x 58.5 mm: it fits a 220 x 220 mm bed.

## Holding the boards in the base

- The pocket walls of the component board and of the charger used to stop 0.2 mm above the underside of the board, so they
  held nothing. They now rise to the top of the board plus 0.4 mm (`enc_top_plq`, `enc_top_car`) and each long wall has
  two small ribs (`crush_w` 0.8 mm wide) that press 0.1 mm (`crush_i`) on the board edge, so the board is held by friction
  and is not loose when the base is alone. There is no flexing hook: neighbouring pockets share their walls. Not tested in
  print: if a board does not go in, lower `crush_i`; if it still moves, raise it (0.2 mm at most).
- The charger board measures 27.8 x 17.4 mm and is 2.6 mm high without the USB-C connector (4.1 mm with it).

## Connectors reaching the outside

- The end of the ESP32 board, the charger board and the microSD board each sit in a recess cut into the inner face of
  the wall, so that only a skin of `conn_skin` (0.8 mm, two perimeters) is left in front of them. Near the rounded
  corner the skin gets thinner (about 0.5 mm at the thinnest), which is why the corner radius `r_esq` is 5 mm.
  The model contains a hidden helper body `Envolvente exterior` (the plain outer prism); it trims anything of the base that
  would stick out of the shell. Do not export it. The openings follow the
  shape of the connector: a trapezoid for the micro-USB (7.4 x 2.8 mm shell, wide side up), a stadium for the USB-C
  (8.7 x 3 mm shell), and a narrow slit for the card (`sd_slot_w` x `sd_slot_h`). Each opening has `conn_c` (0.3 mm) of
  clearance. With a skin of 0.8 mm the 0.5 mm tabs of the micro-USB and the USB-C shell end about 0.3 mm behind the
  outside, a connector flush with its board edge about 0.8 mm. A connector cannot stand further out than that unless the
  board itself passes through the wall; lower `conn_skin` to 0.4 mm if you accept a one-perimeter skin.
- The microSD card sits 0.1 to 0.2 mm beyond its socket, so its edge stays about 0.65 mm inside the wall: push it with a
  fingernail and it springs out. Measured: board 41.7 x 23.8 mm (51.5 with the black header), holes 1.2 mm from the board
  edges (centres 39.3 x 21.4 mm apart), card centre 1.8 mm above the underside and 12.2 mm from the top edge; the slit is
  15 x 1.8 mm for a socket opening of 14.8 x 1.6 mm.
- The two lower ESP32 posts reach into the lip of the cover; a combine feature trims them against the cover.

## Changes after the first full print (2026-11-05)

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

## Change after the second print (2026-11-05)

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

## M3 screws instead of M2 (2026-11-06)

M2 screws were hard to find, so the screw posts are now sized for **M3**. One parameter, `tornillo_pil` (2.5 mm pilot
hole for M3 in PLA), drives the pilots of the OLED, ESP32, XL6009, component board and button board (`oled_pil`,
`esp_pil`, `xl_pil`, `plq_pil`, `plb_pil`). The posts keep their diameters (`poste_d` 5 mm; `esp_poste_d` 4 mm, so the
wall around the ESP32 pilot is only 0.75 mm, below the 1.2 mm rule: if a post splits, lower `tornillo_pil` to 2.4 or widen
`esp_poste_d` if the sockets allow).

- **Boards:** the module holes of the OLED, ESP32 and XL6009 are 3 mm: ream them to 3.2 mm. The component and button
  boards are drilled by the builder: drill them to 3.2 mm. A 3.2 mm hole on a 2.54 mm grid just touches the neighbouring
  pads (keep-out cells in `plaquita.md` still apply).
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

## Engravings

- The `IRIS` text on the front face is only engraved (0.8 mm deep), single colour. There is no separate inlay body.
- Silkscreen labels in the pockets (floor: BATTERY, ESP32, CHARGER, PCB, XL6009, MICROSD, PWR under the switch, IR in
  the LED compartment; inner front wall: OLED, BUTTONS) are 3 mm bold letters engraved 0.4 mm. At 0.4 mm nozzle the strokes are about one extrusion wide, so they may
  look faint; raise `serig_h` or `serig_prof` if they do not read. They are inside the case and are not needed for the
  remote to work.

## Keys

- The four direction keys are rounded triangles pointing away from the centre, and the OK key is a rounded square turned
  45 degrees. Their shape stops them from turning (the play is limited by the 0.25 mm hole clearance), so there is no
  anti-rotation tooth and the direction needs no engraved arrow. The OK key carries the letters OK, engraved 0.6 mm deep
  (`key_prof`, `ok_h` 3.2 mm). Keys print with the flange on the bed and the stem up, so the engraving is on the last layers.
- Sizes (measured switch pitch 7.3 mm, `pad_pitch`): each key is a reference triangle (`tri_b0` x `tri_h0`, 3.6 x 2.7
  mm, centroid on its switch) grown by the corner radius `tec_r` (0.8 mm), about 5.2 mm wide and 4.3 mm tall; the OK key
  is a reference square turned 45 degrees with half-diagonal `ok_d0` (3.2 mm) grown by `tec_r`, 8.0 mm across its
  diagonals. The hole is the key plus `btn_clr` (0.25 mm) and the retaining flange is the key plus `fl_ov` (0.6 mm).
  Gap between the OK key and a triangle 1.6 mm; flange to flange 0.4 mm.
- The OK lettering is `ok_h` 2.2 mm high (about 4.5 mm wide, stroke about 0.43 mm, one extrusion): it is small for a
  0.4 mm nozzle. If it prints too faint, make the keys bigger first (smaller `pad_pitch` is not
  possible with 6 x 6 switches), or leave the lettering out: print one key alone first (about ten minutes).

## Checks done in Fusion, and what was not checked

Done: all features healthy, every body is a single lump, no interference between base, cover and keys,
parameters recompute when a module dimension changes.

Not done: nothing was printed or sliced; wall thickness and bridging were not checked with a slicer; the snap-fit
bumps from the first version were not re-tested at the new size; estimated values are `plq_*` (component board),
`tsop_*`, `esp_usb_z`, `car_usb_z`, `esp_pcb_z`, `sd_post_h`, `xl_post_h`, `oled_vidrio`, `plb_sw_h`. Check them against
your parts before printing.

## OLED washers for M3 x 5 (2026-11-06)

Only M3 x 5 and M3 x 6 screws were available, but the OLED posts were drawn for M3 x 4 (pilot 2.3 mm). A printed washer
under each OLED screw head makes up the difference: `arandela_h` = `arandela_tornillo_disp` (5) - `arandela_tornillo_dis` (4)
= 1 mm, outer diameter `arandela_od` 6.4 mm, hole `arandela_id` 3.4 mm. Four washers sit in the model, off to the side
(x from `arandela_x`, pitch `arandela_paso`), as separate bodies "Arandela OLED 1..4"; a case export must skip them or
send them to their own folder. Print them flat, 4 pieces, 100 % infill.

## Thicker ESP32 posts (2026-11-06)

With the 2.5 mm pilot the 4 mm ESP32 posts left a 0.75 mm wall, which risks splitting when the M3 screw cuts its thread. The
parameter `esp_poste_d` is now `2 * 1.25 mm + tornillo_pil` (5.0 mm now), so the wall stays at 1.25 mm whatever the pilot is.
Check when assembling that the wider posts do not touch the header pins under the ESP32.

## End lip removed at the ESP32 end (2026-11-06)

The cover lip along the ESP32 end wall (the strip that was left on both sides of the earlier notch) still stopped the cover
from closing, so it is cut away completely: cut "Corte Quitar lengueta extremo ESP32", from the lip bottom to the seam, across the
whole inner width less `esp_leng_borde` (0.5 mm) of side wall. The side lips and rounded corners stay, so the cover keeps its
centring along the long sides. The ESP32 is held by its screws only.

The ledge that was still there (2026-11-06, second try): the ESP32 wall window of the cover left a 0.4 mm-thick inner strip under it
(z 17.0 to 17.4), right under the board. The window now goes down to `z_union - 0.2 mm`, so that strip is gone and only the outer
1.2 mm skin remains over the ESP32 width.

## microSD module 1 mm wider (2026-11-06)

The microSD board measured 23.7 mm on its short side, not 22.8, so it did not fit. `sd_ancho` is now 23.8 mm (23.7 + 0.1 clearance). The
hole pitch across (`sd_hy`, now 21.4 mm), the pocket and the hold-down ribs follow the parameter; the edge on the OLED side stays and the
other edge moves 1 mm toward the IR end.
