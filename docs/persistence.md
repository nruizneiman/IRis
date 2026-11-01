# Persistence (Stage 6)

Source: `lib/iris_core/src/persistence/`, `app/Boot.*`. Tests: `test/unit/test_persistence.cpp`,
`test_power_cut.cpp`, `test_boot.cpp`. The store itself (`IBlobStore`) is LittleFS on the device
(`src/infrastructure/esp32/PersistentStore.h`) and an in-memory fake on the PC.

## Why one file per remote

Parsing JSON needs RAM proportional to the file. A single library file at the configured limits would
not be safe on an ESP32 (about 200-250 KB of free heap, unverified). With one file per remote the
peak is bounded by the largest remote (<= 40 buttons, <= 4000 RAW entries): measured on the PC with
a counting allocator the worst case is below 96 KB for text + parsed document, and the 64-bit PC
overestimates the 32-bit device (`worst-case remote file stays within the RAM budget`). The firmware also
checks `ESP.getMaxAllocHeap()` before saving (`UiContext::persist`) and the Diagnóstico screen prints
`Heap libre / mínimo` so the assumption can be checked on the board.

## Layout

| Key | Content |
|---|---|
| `idx.irs` | Index: counters, categories, remotes (name, category, file key, file CRC) |
| `idx.prev` | Previous index = one generation of backup |
| `r<id>g<gen>.irs` | Buttons and signals of one remote |
| `settings.cfg` | Preferences (CRC-protected) |
| `boots.log` | Last 12 boots with their reset reason (diagnostic, damage tolerated) |
| `explore.log` | Log of the last exploration run |
| `bad<id>.irs`, `badidx.irs` | Unreadable bytes kept for inspection |

Every file starts with `IRIS <version> <crc32> <length>` and a JSON body. RAW durations are hex text
(4 digits per entry); default-valued fields are omitted.

## Commit protocol

`save()` writes only the remote files whose content changed, under a **new generation** name, and then
replaces the index. The index replacement is the commit: `IBlobStore::write()` is atomic (LittleFS:
temporary file, size check, rename), so a power cut leaves either the old library or the new one.
Before replacing the index the previous one is copied to `idx.prev` (only if it is still intact, so damage
is never promoted to a backup). Files referenced by neither index are orphans and are deleted after a
successful commit. `test_power_cut.cpp` interrupts a save before every possible write and checks that
the restarted device finds a complete old or new library.

## What loading does

| Situation | Result |
|---|---|
| Nothing stored | Empty library |
| Index intact | Library; files checked against the CRC in the index |
| Index corrupt, backup index intact | Backup (boot toast "Se usó el respaldo") |
| One remote file damaged / missing | Everything else loads; the remote is reported ("Perdido: Sala") and its bytes kept as `bad<id>.irs` |
| Both indexes corrupt | Empty library, bytes kept as `badidx.irs`, saving allowed ("Datos dañados: vacío") |
| Read error or out of memory | **Saving disabled** until reboot; nothing is written or deleted ("No se pudo leer: sin guardar") |
| Data written by a newer firmware | **Saving disabled**; no fallback to an older backup ("Datos de otra versión") |
| Format-1 file (`library.irs`) | Loaded; migrated on the next save and then removed (golden file in `test/fixtures/library_v1.irs`) |

A transient failure is never treated as corruption: that would overwrite good data with an empty library.

## Versioning

`kFormatVersion = 2`. Format 1 (single file, RAW as number arrays) is read by a dedicated loader
frozen by a golden fixture; any future change must add a new reader step and a fixture for the format it
replaces. Files with a higher version are refused, never guessed.

## Flash wear (estimate, unverified)

A save rewrites the index (about 3-6 KB) plus the remotes that changed (up to roughly 25-45 KB each in the
worst case) and the backup index. With LittleFS wear levelling over a ~1.4 MB partition and a typical
edit touching one remote, 10 000 saves is roughly a few hundred full-partition writes, far below the
100 000 erase cycles usually quoted for NOR flash. This is arithmetic, not a measurement.

## Not covered

- A power cut inside `rename()` itself is the file system's responsibility (littlefs documents it as atomic);
  only a real power-cut test can confirm it on this board (`docs/hardware.md`).
- Several wrongly-damaged remote files at once are reported but not repaired.
- No export/import of the library to a PC yet.
