# Documentation

Start with the [project README](../README.md): what IRis is, how to flash it and how to use it.

## Building one

Read them in this order.

| Document | What it is |
|---|---|
| [Parts list](hardware/bom.md) | Every part, with quantities, screws and printed pieces |
| [Wiring](hardware/wiring.md) | The circuit: connection diagram, pin map, each block and the safety rules |
| [Component board](hardware/component-board.md) | The small soldered board: hole map, where each part goes, which wire goes to which pad |
| [Case](hardware/case.md) | The printed case: files, print settings, how the modules are held, how to adapt it |
| [Bring-up](hardware/bring-up.md) | Powering up a new unit one block at a time, with what to expect and what to do if it fails |
| [Bring-up log](hardware/bring-up-log.md) | A template to record the results |

Background, for when your parts differ or something does not behave:

| Document | What it is |
|---|---|
| [Design notes](hardware/design-notes.md) | Why the circuit is this way: choosing the transistor, sizing the LED resistor, charging and regulation options |
| [Measurements](hardware/measurements.md) | What was measured on the author's unit, and what was not |

## Using it

| Document | What it is |
|---|---|
| [Interface](firmware/ui.md) | The keys, the screen map, remotes from the card, the modes and the limits |

## Working on the firmware

The layout of the code and how to run the tests are in the [project README](../README.md#developing).

| Document | What it is |
|---|---|
| [Interface](firmware/ui.md) | The rules the screens follow, the screen grid, the translation catalog |
| [IR layer](firmware/ir.md) | How signals are sent and received, the library calls used, the emitter limits |
| [Persistence](firmware/persistence.md) | The storage format, the commit protocol, recovery from damage |
| [Dependencies](firmware/dependencies.md) | The versions the project is built with, and the licence of the remote database |

## Where the project stands

| Document | What it is |
|---|---|
| [Status](status.md) | What works, and how each part was checked |
| [Limitations](limitations.md) | What is unknown, limited or assumed |

## Other folders

| Folder | What it holds |
|---|---|
| [`img/`](img) | Pictures used by the documents |
| [`schematic/`](schematic) | The connection diagram as a KiCad file and as a PDF |
| [`../case/`](../case) | The 3D model of the case |
| [`../wokwi/`](../wokwi) | A simulation project that was prepared and never validated |
