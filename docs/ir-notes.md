# IR layer notes (Stage 5)

What was verified, how, and what the findings mean. "Cross-check" = `test/hostlib` (IRremoteESP8266 2.9.0
compiled for the PC in its `UNIT_TEST` mode; encode with `IRsendTest`, decode with `IRrecv`). It proves
the app's mapping agrees with the library; it proves nothing about a real receiver, LED or carrier.

## Library API actually used (read from the 2.9.0 sources)

| Need | Call |
|---|---|
| Send a known protocol | `IRsend::send(type, data, nbits, repeat)` with `data` from `encodeNEC / encodeSAMSUNG / encodeSony / encodeRC5(X) / encodeJVC` (non-static members) |
| Copies a protocol needs | `IRsend::minRepeats(type)` (Sony: 2 extra copies) |
| Send RAW | `IRsend::sendRaw(uint16_t buf[], len, hz)` - first entry is a mark |
| Receive | `IRrecv(pin, bufsize, timeout, save_buffer=true)`, `enableIRIn()`, `decode(&results)`, `disableIRIn()` |
| Raw capture units | `results.rawbuf[i] * kRawTick` (2 us per tick); `rawbuf[0]` is the idle gap, dropped |
| Repeat codes | `results.repeat` (NEC repeat code, JVC repeat): not learned as a new signal |

`IRsend::ledOff()` is protected, so the adapter drives the pin low itself.

## Findings that changed the domain model

1. **Sony 20-bit**: the library has 5 address bits and 8 "extended" bits that it reports inside the command
   (command = 7 + 8 = 15 bits). The Stage 2 catalogue said 13-bit address / 7-bit command; fixed to `{20, 5, 15}`.
2. **RC5X with command < 64 is RC5**: the second start bit is the inverted 7th command bit, so a 13-bit frame
   with command < 64 is identical on the wire to the 12-bit frame and is always reported as 12 bits.
3. **NEC extended addresses `0xAB..` where the high byte is the inverse of the low byte** (e.g. `0xF20D`) are
   identical on the wire to the standard 8-bit address (`0x0D`) and decode as such. Addresses <= `0xFF` are
   always sent as standard NEC.

## Behaviour decisions

- The learn flow keeps the full RAW capture even when the protocol is recognised.
- Held-key repeat codes and overflowed buffers never become a "recognised" capture.
- The receiver ignores everything while the emitter is on air and for 40 ms afterwards (own signal / echo).
- Transmissions run in a FreeRTOS task on core 0 (UI on core 1). `cancel()` takes effect between library
  calls: a frame, or a Sony burst of 3 copies, that is already on air is finished.
- Frame carrier: protocol frames use the library's per-protocol frequency/duty (not a universal 38 kHz).
  RAW frames use the frame's carrier, or the job default (38 kHz) when unknown. A demodulating TSOP38238
  cannot report the original carrier, so replaying a learned RAW signal at 38 kHz is an assumption.
- Software PWM (`mark()` busy loops) is sensitive to interrupt jitter; the real waveform must be checked
  with the physical tests below.

## Emitter safety limits (PROVISIONAL, `TxSafety` in `IrTransmitterAdapter.h`)

| Limit | Value | Why |
|---|---|---|
| Longest single mark | 25 ms | A bug must not hold the LED on; NEC leader is ~9 ms |
| Estimated job length | 3 s | Bounds exploration bursts and RAW playback |
| Quiet time after a job | 5 ms | Gives the supply and the LED time to recover |
| Emitter channels | channel 0 only | One MOSFET stage exists |

None of these were derived from the LED or the MOSFET stage (neither is identified). Revisit after the
physical tests; do not raise them before knowing the LED's pulse rating and the supply behaviour.

## Not done / not verified

- No code was run on the ESP32. Timer conflicts (IRrecv uses hardware timer 3), watchdog behaviour during
  long busy-wait frames, and core-0 scheduling are unverified.
- IRremoteESP8266 compiles all protocols (flash use 47 %); disabling unused ones (`-DSEND_xxx=false`) is
  possible later if flash gets tight.
- State-based protocols (air conditioners) have no model yet: `DecodedFrame::state` exists but the
  catalogue does not use it.
