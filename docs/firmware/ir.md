# IR layer

How the firmware sends and receives infrared, what was verified and how.

The firmware uses the IRremoteESP8266 library (2.9.0) through `lib/iris_ir`, which maps the domain model of the app to
the library. That mapping is checked by the **cross-check** tests in `test/hostlib`: the library is compiled for the PC
in its `UNIT_TEST` mode, a signal is encoded with `IRsendTest` and decoded with `IRrecv`. The cross-check proves that
the mapping agrees with the library; it proves nothing about a real receiver, LED or carrier.

## Library calls used (read from the 2.9.0 sources)

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
   (command = 7 + 8 = 15 bits). The catalogue of the app therefore describes it as `{20, 5, 15}`.
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
- Software PWM (`mark()` busy loops) is sensitive to interrupt jitter. The real waveform was never looked at with
  an instrument ([bring-up](../hardware/bring-up.md), block B6).

## Emitter safety limits

`TxSafety` in `src/infrastructure/esp32/IrTransmitterAdapter.h`. Provisional values.

| Limit | Value | Why |
|---|---|---|
| Longest single mark | 25 ms | A bug must not hold the LED on; NEC leader is ~9 ms |
| Estimated job length | 3 s | Bounds exploration bursts and RAW playback |
| Quiet time after a job | 5 ms | Gives the supply and the LED time to recover |
| Emitter channels | channel 0 only | One driver stage exists |

None of these were derived from the ratings of the LEDs or from a measurement: they only bound what a software bug
could do. Do not raise them before measuring the LED current and the behaviour of the supply while sending.

## On the device

- Learning and sending are in daily use on the author's unit. Only NEC remotes were confirmed there; Samsung, Sony,
  RC5 and JVC are covered by the cross-check on the PC only.
- No timer conflict, watchdog reset or scheduling problem was seen in use, but none was looked for with an
  instrument: the carrier frequency and the frame timing were never measured.

## Not done

- IRremoteESP8266 compiles all its protocols. Disabling the unused ones (`-DSEND_xxx=false`) is possible if flash
  gets tight.
- State-based protocols (air conditioners) have no model yet: `DecodedFrame::state` exists but the
  catalogue does not use it.
