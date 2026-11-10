# Component board ("plaquita"): size, hole map and wiring

Status (2026-11-04): **designed, not built.** The hole map below was checked only against a few layout rules (no part on a
screw keep-out cell, no part body over another part's lead, no unintended neighbouring pads of different nets). Nothing was
tested electrically. Check polarities and every neighbour of a critical pad with a multimeter before connecting the battery.

## The board

- Pad-per-hole perfboard, 1.2 mm thick (`plq_t`), 2.54 mm pitch, copper on one side only. Components go on the bare side,
  solder joints on the copper side, which faces the screw posts of the case.
- The board as actually cut is **45.9 x 28.8 mm** (18 rows x 11 columns A to K; the right edge was filed straight, it was
  28.9 to 29.5 mm before). The case pocket follows it: `plq_largo` 45.9, `plq_ancho` 28.8, with 0.4 mm of clearance per
  side that the friction ribs take up. The screws are inside the board outline: there is no extra margin to add for them.
- The posts are 20.32 mm apart across and 38.1 mm along. Across, they sit `plq_hole_inset_x` from the pocket edge
  (4.24 mm, assuming the 11 columns are centred in the 28.8 mm width); along, `plq_hole_inset` (3.81 mm). If your cut is
  not centred, change the parameters.
- Mount with **M3 x 5 mm** screws (the case was first drawn for M2 x 5; see `case-print.md`) in four holes drilled at
  **3.2 mm** in cells **B2, J2, B17 and J17** (the second hole from each corner; the holes are 20.32 mm apart across
  and 38.1 mm along). A 5.5 mm head covers the cells next to the hole, which are free anyway.
- Keep the five cells around each screw hole free of solder (the hole and its four orthogonal neighbours): the plastic
  post of the case rests on the copper side and a solder blob there would lift the board. Diagonal cells are fine.
- The pocket walls reach the top of the board, and two small ribs per long wall press 0.1 mm on its edge, so the board
  clicks into the pocket before it is screwed down.
- Row 1 is the edge toward the IR LEDs and the XL6009, row 18 the edge toward the charger and ESP32.

## Hole map

```
    A B C D E F G H I J K
 1  . x . 1 . 2 . 3 . x .      1, 2, 3: LED anode pads (3rd is a spare branch)
 2  x O x - . - . - x O x      O: screw hole, x: keep free, -: body of a part sitting over holes
 3  . x . - . - . - . x .
 4  . . . - . - . - . . .
 5  V V . V V V . V . . V      V: VIN (5 V) rail, row 5
 6  . . . - - - . . . . .
 7  . . . - - - . . . . .
 8  G G G . G . . . G G G      G: GND rail, row 8
 9  . . G - - - M . . . .      M: divider midpoint (to GPIO33)
10  . . G - - - M . K B G      K: collector / LED cathodes, B: transistor base
11  . . . . . . M . . B G      S: sense input (from the XL6009 input +)
12  . . S - - - M . . B .      T: GPIO23 input
13  . . . . . T - - B . .
14  . . . . . . . . . . .      rows 14 to 18 (and the free cells of row 13): free, for a second driver and more LED branches
15  . . . . . . . . . . .
16  . x . . . . . . . x .
17  x O x . . . . . x O x
18  . x . . . . . . . x .
```

## Parts and where they go

| Part | Leads in | Notes |
|---|---|---|
| R_a1, 100 ohm | D5 and D1 | VIN to LED 1 anode. Stands along column D (body over D2 to D4) |
| R_a2, 100 ohm | F5 and F1 | LED 2 |
| R_a3, 100 ohm (optional) | H5 and H1 | spare LED branch |
| C_bulk, 100 uF | **E5 (+)** and E8 (-) | stands over D6 to F7; mind the polarity |
| R_d2, 10 kohm | C10 and G10 | divider bottom, lies along row 10 |
| R_d1, 10 kohm | C12 and G12 | divider top, lies along row 12 |
| C_f, 100 nF | C9 and G9 | across the divider bottom resistor |
| Q1, 2N2222 | C = I10, B = J10, E = K10 | flat face toward the top edge (row 1); read from the flat face left to right it is E, B, C |
| R_pd, 100 kohm | J11 and K11 | base to ground, stands upright |
| R_b, 1 kohm | I13 and F13 | GPIO23 to base, lies along row 13 |

## Connections on the copper side

Solder bridges between **adjacent** pads of the same net:

- GND: C8-C9-C10 (this joins the divider and the rail), K10-K11 (emitter and R_pd).
- Divider midpoint: G9-G10-G11-G12.
- Base: J10-J11-J12.

Bare wire rails:

- VIN: along row 5 from A5 to K5 (solder the pads that are used: A5, B5, D5, E5, F5, H5, K5).
- GND: along row 8 from A8 to E8 (pads A8, B8, C8, E8; D8 is passed over, not soldered), then an **insulated jumper from
  E8 to I8** passing over F8, G8, H8, then bare wire I8 to K8. **Do not solder G8, F8 or H8**: G8 is next to the midpoint
  pad G9.

Two short bare-wire links on the copper side: **K10 to K8** (transistor emitter to the GND rail, straight along column K;
K9 becomes GND and stays unused) and **I13 to J12** (R_b to the base), made as an L-shaped wire over I13, J13 and J12
(J13 becomes part of the base net; keep K13 free).

## Wires to the rest of the case (soldered)

| Pad | Goes to |
|---|---|
| A5 | XL6009 OUT+ |
| B5 | ESP32 VIN |
| K5 | microSD VCC |
| A8 | XL6009 OUT- |
| B8 | ESP32 GND |
| I8 | OLED GND, and from there TSOP GND |
| J8 | microSD GND, and from there the common GND of the five buttons |
| D1, F1 (H1) | IR LED anodes |
| I10 | both IR LED cathodes together |
| F13 | ESP32 GPIO23 |
| G11 | ESP32 GPIO33 |
| C12 | XL6009 IN+ (battery side, after the power switch) |

The ESP32 3V3 does not go through the board: ESP32 3V3 to the OLED VCC, and from the OLED VCC to the TSOP VS.

## Cautions

- **C12 is wired to the battery positive.** It must never touch a GND, VIN or divider pad. The layout leaves it isolated
  (no soldered pad in the cells next to it); keep it that way, and put a drop of hot glue or heat shrink on the wire joint.
- **I10 (LED cathodes) must not touch the divider midpoint**: with the transistor off that node floats up toward 5 V and
  GPIO33 accepts at most 3.3 V.
- The pairs J10-K10, J11-K11 and I10-J10 are neighbours because they are the transistor pins and the two leads of R_pd:
  keep their solder joints small.

## Room for more LEDs and transistors

- The circuit above leaves rows 14 to 18 free (about 45 usable holes, between columns C and I): enough for one more
  2N2222 with its 1 kohm and 100 kohm resistors and two more 100 ohm LED resistors. Row 14 is left empty on purpose so that
  a new pad never sits next to the GPIO23 pad (F13) or the base pad (I13).
- Each IR LED needs its own 100 ohm resistor between VIN and its anode. R_a1 and R_a2 are used; **R_a3 (H5 and H1) is
  already a free slot** for a third LED, and one 2N2222 can drive the three of them (about 100 mA calculated, not
  measured). The case reads the number of LEDs from the parameter `led_n` (2 now, up to 4 without touching the TSOP window).
- For a fourth LED or more, build the second driver in rows 15 to 18: put the new transistor, its 1 kohm (to GPIO23 or to
  GPIO19, which is reserved for a second channel) and its 100 kohm (base to ground) there, the emitter to the GND rail with a
  jumper to a spare pad of that rail (D8, or a second wire on I8, J8 or K8; F8, G8 and H8 stay unsoldered), the new LED resistors to VIN with a jumper from a spare pad of
  the VIN rail (C5, G5, I5 or J5), and a separate wire from its collector to the extra cathodes. Keep one empty row between
  the new pads and any other net, as in the map, and do not put a pad next to the sense pad (C12).
