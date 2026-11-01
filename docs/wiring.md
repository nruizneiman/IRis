# Wiring guide

Status: **design guidance, nothing here has been built or measured.** Facts marked *(datasheet)* were read in
the manufacturer document named next to them; facts marked *(to verify)* are assumptions you must check
before relying on them. Values given as "starting point" are not final: the LED, resistors, MOSFET and power
modules of this project are not identified yet. Follow `docs/physical-test-plan.md` block by block; do not wire
everything at once.

The firmware pin numbers come from `include/board_pins.h` (the only place with GPIO numbers).

## 0. Safety rules

1. **Never connect an IR LED (or any load) directly to a GPIO.** The emitter is always driven through a MOSFET.
2. **The first tests are powered from USB only.** The battery, charger and boost converter are added last, one at
   a time (blocks B9 in the test plan).
3. **Li-ion cells store a lot of energy.** Use a protected cell or a holder module with protection, check polarity
   before connecting, never short the terminals, never use a cell that is swollen, dented or hot, and keep the
   first charges attended on a non-flammable surface.
4. Never feed 5 V into a pin labelled 3V3 or into any GPIO. The ESP32 GPIOs are **3.3 V only**.
5. Switch power off before changing wiring. Check polarity with a multimeter before applying power to a new block.
6. Keep every wire of the LED branch short and solid; do not trust breadboard rails for currents above a few tens
   of milliamps (they have contact resistance that shows up as voltage drop and resets).

## 1. What must be identified before wiring

| Part | What is known | What is NOT known | How to find out (test-plan block) |
|---|---|---|---|
| ESP32-WROOM-32 dev board, 30 pins, USB-C | Module family | Exact pin labels of your board | Compare silkscreen with section 2 (B2) |
| OLED 1.3", 128x64, I2C | Size, interface | **Controller: SH1106 or SSD1306**; I2C address | `display_probe` (B3) |
| 2x TSOP38238 | 38 kHz demodulating receiver | Whether your parts are genuine TSOP38238 | Marking + behaviour (B5) |
| IR LEDs | Several, apparently identical | **Model, forward voltage, current/pulse ratings, peak wavelength** | Markings, measurement (B1, B7). Do not assume a model |
| Resistors | Some values unconfirmed | Real values | Measure out of circuit (B1) |
| 3x IRFZ44N | N-channel power MOSFET | Not specified for 3.3 V gate drive (see 4.5) | Measure Vds at the real current (B6) |
| 18650 cell + 1-cell holder | Nominal 3.6/3.7 V | Protected or not, capacity, condition | Markings, voltage (B1, B9) |
| USB-C charger module for 1 cell | "Protected" per listing | Chip (TP4056?), whether it has power-path | Chip marking (B1, B9) |
| DC-DC module "XL6009" | Looks like an XL6009 boost | Exact model, adjustment, **minimum input voltage** | Markings + bench test (B9) |

## 2. Pin map (from `include/board_pins.h`, **unverified on your board**)

| Function | GPIO | Notes |
|---|---|---|
| I2C SDA / SCL | 21 / 22 | ESP32 default I2C pins |
| IR receiver 1 / 2 | 34 / 35 | Input-only pins, no internal pull-up (the TSOP output has its own). Receiver 2 is not enabled in firmware |
| IR emitter gate 1 / 2 | 23 / 19 | Gate 2 is reserved for a second emitter channel (not implemented) |
| Buttons Up / Down / Left / Right / OK | 13 / 14 / 27 / 26 / 25 | Each button between the GPIO and GND, internal pull-up, active low |
| Battery sense (optional) | 33 | ADC1; only if you build the divider of 4.6 |

Avoided on purpose: GPIO 6-11 (connected to the module's flash), 0 / 2 / 5 / 12 / 15 (boot strapping; GPIO12 held
high at reset selects 1.8 V flash voltage and can stop the boot), 1 / 3 (USB serial). A typical 30-pin ESP32 dev
board exposes D13, D14, D27, D26, D25, D23, D19, D21, D22, D34, D35, D33 *(to verify against your silkscreen: 30-pin
boards differ and some do not expose every pin)*.

## 3. System overview

```
                         +-------------------------- USB-C (bench: only source in tests B2..B8)
                         |
   [18650]--[switch]--[protected charge module]--+--> LED branch supply (battery rail, up to 4.2 V)
                                                 |
                                                 +--> regulator --> ESP32 board (3.3 V logic)

   ESP32 3V3 ---- OLED, TSOP receivers (low current, clean rail)
   ESP32 GPIO23 --[Rg]--> MOSFET gate ... LED branch is a separate current loop (section 4.5)
   One common ground "star" point joins: ESP32 GND, MOSFET source, LED-branch capacitor, battery negative.
```

Three separate concerns, three separate sections: logic (4.1-4.4), emitter power stage (4.5), battery / charging /
regulation (4.7).

## 4. Block wiring

### 4.1 ESP32 board on USB

USB-C cable (a *data* cable, not charge-only) from the PC. Nothing else connected. Check with a multimeter that
the board's 3V3 pin reads about 3.3 V and that GPIO23 reads about 0 V after boot.

### 4.2 OLED (I2C)

```
OLED VCC -- ESP32 3V3        OLED SDA -- GPIO21
OLED GND -- ESP32 GND        OLED SCL -- GPIO22
```

Most modules carry I2C pull-up resistors already; add none unless the I2C scan (`bench`, command `i`) fails and
you have confirmed the module has none (use roughly 4.7 kΩ to 3V3, *to verify*). Pin order on these modules
varies (GND-VCC-SCL-SDA or VCC-GND-SCL-SDA): read the labels, do not assume.

### 4.3 Buttons

```
GPIO13 ---o/ o--- GND      Up          GPIO27 ---o/ o--- GND   Left
GPIO14 ---o/ o--- GND      Down        GPIO26 ---o/ o--- GND   Right
GPIO25 ---o/ o--- GND      OK
```

No external resistors are needed (internal pull-ups, 30-50 kΩ typical). Keep the wires to the switches short; a
100 nF capacitor across a switch is optional if long wires pick up noise. The 5 switches and the power switch
are different things: the power switch cuts the battery (4.7), it does not go to a GPIO.

### 4.4 IR receivers (TSOP38238)

*(Vishay datasheet TSOP382.., TSOP384.., Rev. 2.1, document 82491.)*

- Pinning: **1 = OUT, 2 = GND, 3 = VS.** Use the package drawing in the datasheet to find pin 1: do not rely
  on memory or on the order of a breakout board's silkscreen.
- Supply voltage 2.0 V to 5.5 V, absolute maximum 6 V; supply current about 0.35 mA at 3.3 V. Output current maximum 5 mA.
- The output is active low and has an **internal 30 kΩ pull-up** (block diagram), so it connects directly to a GPIO
  of the ESP32 when the receiver is powered from 3.3 V.
- R1 and C1 in series with VS are "recommended in case there are strong ripple or spikes on the supply line"
  (the datasheet shows them; this guide does not give values because they were not readable in the text used).
  A bypass capacitor of 100 nF close to the receiver is harmless; add an R-C filter if captures show noise when
  the emitter or the display is active.

```
ESP32 3V3 ---[R1?]---+--- VS (3)          OUT (1) ----------- GPIO34  (receiver 1)
                  [C1?]=                                      GPIO35  (receiver 2, later)
ESP32 GND ----------+--- GND (2)
```

Placement: facing outwards, away from the emitter LEDs and shielded from their light, otherwise the receiver is
blinded by the device's own transmissions (the firmware also ignores captures during and just after a transmission).

Behaviour to know *(datasheet)*: this version (AGC for long bursts) needs at least 10 carrier cycles per burst, a
gap of at least 12 cycles after short bursts and a longer gap after long bursts, and it suppresses continuous
signals. NEC and RC5/RC6 are listed as supported; for Sony 12/15/20-bit codes the datasheet points to a different
part (TSOP38S40), so **Sony captures may be less reliable with TSOP38238**. Raw mark/space durations measured
through it are approximations of the transmitted ones.

### 4.5 IR emitter stage (MOSFET + LED)

#### Schematic (starting point, values to be chosen in tests B6-B7)

```
 LED-branch supply (+)  ----+-----------------+
  (3.3 V / 5 V for tests,   |                 |
   battery rail later)   [C_bulk]          [R_led]            R_led: computed from your measured Vf (below)
                            |                 |
                            |              LED anode
                            |                 |
                            |              LED cathode
                            |                 |
                            |              MOSFET drain
                            |                 |
 GPIO23 --[Rg]--+----------------------- MOSFET gate      MOSFET source ---+
                |                                                          |
              [Rpd]                                                        |
                |                                                          |
 GND star ------+----------------------------------------------------------+------ battery(-) / USB GND
```

- **Rg** (gate resistor): a starting point is 100 Ω to 220 Ω. It limits the gate charging current and ringing.
- **Rpd** (gate pull-down): about 100 kΩ from gate to GND. It keeps the MOSFET off while the ESP32 is in reset or
  booting, when the GPIO is high impedance. It must be present before the LED is ever powered.
- **C_bulk**: 100 µF to 470 µF, low ESR, plus 100 nF ceramic, soldered close to the LED/MOSFET and returned to the
  star point. Reason: the carrier switches on/off every 26 µs (38 kHz) and a current of I amps for 26 µs / 3 times
  the allowed droop dV needs roughly C = I·t/dV; the library's default 33 % duty means about 8.7 µs of on-time per
  period, so for 0.1 A and 50 mV that is about 17 µF; a few hundred µF is comfortable *(estimate, not measured)*. It does not help against the slow sag of a weak battery over
  a 10 ms burst: that is the battery's job.
- A freewheel diode is not needed (the load is not inductive).
- The ESP32 GND, the MOSFET source and the capacitor return should meet at one point; do not return the LED current
  through the thin ground trace/wire that feeds the ESP32.

#### Choosing the MOSFET

The ESP32 can only drive the gate to 3.3 V. Criteria: N-channel; a datasheet that specifies Rds(on) at a gate
voltage not above 3.3 V, or whose gate-threshold maximum plus a safe margin is clearly below 3.3 V and whose
transfer curve (drain current versus gate voltage) shows several times your LED current at 3.3 V; drain-source
rating well above the supply; the package you can wire.

- **IRFZ44N** *(datasheet values reported by distributors)*: gate threshold 2 V to 4 V, Rds(on) of 17.5 mΩ
  **specified at VGS = 10 V**. At 3.3 V a unit with a high threshold may barely turn on. It is not the recommended
  part for 3.3 V drive. Your three units can still be measured (test B6); use one only if Vds stays small at your
  real LED current on **every** unit, and keep it as a fallback.
- **IRLZ44N** (the logic-level sibling): threshold about 1-2 V, Rds(on) quoted at 10 V and 5 V in the sources
  found; a 3.3 V figure was not found, so check its datasheet's transfer curve before relying on it.
- Other candidates mentioned in the project notes (AO3400A, IRLML6344, small logic MOSFETs): I have **not**
  verified their datasheets here; read them first.

#### LED resistor (R_led)

For a continuous (DC) current I through the LED when the MOSFET is on:

```
R_led = (V_supply - Vf - Vds_on) / I_target        P_R = I^2 * R_led   (at 100 % duty; real duty is lower)
```

- `V_supply` is the highest voltage the branch will see (4.2 V on a fully charged cell, 5 V from USB, 3.3 V from the board).
- `Vf` is the LED's forward voltage **at your current**. Measure it (test B1/B7); a multimeter's diode mode applies
  only about 1 mA, so it *under*-reads Vf at tens of mA. Using that lower Vf in the formula gives a larger R and so a
  lower real current: the safe direction for first tests.
- `Vds_on` is the MOSFET drop measured in B6 (tens of mV for a good part).
- Worked example with **assumed** numbers, not your LED: V_supply = 5 V, Vf = 1.2 V, Vds_on = 0.05 V, target 20 mA
  gives R = 3.75 V / 0.02 A = 187 Ω; the next standard value 220 Ω gives about 17 mA.
- Because the rating of your LEDs is unknown, the plan limits the first tests to about 20 mA of **DC-equivalent** current
  (a conservative figure for generic 5 mm IR LEDs, *not* a statement about yours). The average current during a
  38 kHz burst is lower (the firmware's carrier duty is about one third), but size the resistor as if the LED
  stayed on, so a software bug cannot overload it. Raise the current only after finding the LED's datasheet or
  a verified equivalent.

#### Several LEDs later (range / directions)

- Give **each LED its own series resistor** and put the branches in parallel across the same supply (never share one
  resistor between parallel LEDs: unequal Vf makes the current unequal). Or put LEDs in series if the supply is high
  enough (Vsupply > sum of Vf + resistor drop), which gives the same current through all.
- One MOSFET can drive all branches if the **total** current and its heating are within the part's ratings; otherwise
  use the second channel (GPIO19) with its own MOSFET (not implemented in firmware: `IrTransmitterAdapter` accepts
  channel 0 only).
- Re-measure supply sag (B9) every time a branch is added. More LEDs means more current from a battery with
  internal resistance, and that is what causes resets.

### 4.6 Battery voltage divider (optional)

```
battery(+) --[100 kΩ]--+--[100 kΩ]-- GND          GPIO33 --- middle point, with 100 nF to GND
```

Reads half the battery voltage (at most about 2.1 V at 4.2 V, inside the ADC range). It draws about 21 µA all the
time. The ESP32 ADC is non-linear and needs calibration against a multimeter. The firmware reads this pin **only when
built with `-DIRIS_BATTERY_RATIO_PERMILLE=<ratio x 1000>`** (2000 for two equal resistors; `AdcBatteryMonitor`, median of
9 samples, `analogReadMilliVolts` factory calibration, *not verified on the board*). Without the flag the home screen
keeps showing `Bat n/d`, because a floating pin would show noise. With it the title bar shows the voltage (`3.87 V`); no
percentage is invented. Use the bench command `a` to compare the pin millivolts with a multimeter first.

Only 1 kΩ and 10 kΩ parts are on the bench (no 100 kΩ): two 10 kΩ give the same ratio at 210 µA instead of 21 µA, about
0.2 mA against the roughly 80 mA of the running board; take the divider from the **switched** side (after the power switch)
so it never drains the cell while the device is off. Put 100 nF from the pin to GND if you have it.

### 4.7 Battery, charging and regulation

Three jobs that must not be confused: **charging the cell**, **protecting the cell**, and **making 3.3 V**.

**Protected charge module (TP4056-class, with a DW01 + 8205 protection pair).**
- Connect the **cell to B+ / B-**; connect the **load to OUT+ / OUT-** (the protected output). A load wired to B+/B-
  bypasses the over-current / over-discharge protection.
- A TP4056-class charger ends the charge by watching the current fall to a fraction of the charge current. A load
  connected to the cell while charging disturbs that measurement, so the charge may not terminate; the protection
  chip does not fix this. Therefore **do not run the device from the module while it charges** unless you add a
  real power-path (load-sharing) stage.
- Options, simplest first:
  1. **Device off while charging.** Put the power switch in the OUT+ line so the load is disconnected when you
     switch off; charge only with the switch off. Cheap and safe; the limitation is documented in the UI notes.
  2. **Power-path charger module** that states load-sharing / power-path in its datasheet (a charger IC such as the
     MCP73871 is one option mentioned in the sources consulted; *I did not verify a specific module*). The ESP32
     system then hangs from the system output of that module.
  3. **Discrete power-path** (a P-channel MOSFET that disconnects the battery from the load when USB is present, plus a
     Schottky diode on the USB side). A concept only; it needs its own validation before use.

**Making 3.3 V from one cell (3.0 V to 4.2 V).** The cell goes below the 3.3 V that the board needs, so a plain
regulator that only steps down is not enough over the whole range.
- *Boost to 5 V into the board's VIN/5V pin*: the board's own regulator then makes 3.3 V. The module you have
  may be an XL6009, a **boost** converter. Its minimum input is **not settled**: one datasheet revision lists
  3.6 V to 36 V and a newer one 5 V to 32 V, and some module vendors say it does not work below 5 V. Do not assume it can
  start from a 3.7 V cell; test it (B9) with a resistive load before connecting the ESP32.
- *Buck-boost to 3.3 V into the 3V3 pin*: works across the cell range, but then 3V3 must **not** be fed by USB at the
  same time (the board's regulator and the module would fight). Disconnect one source, or isolate with a diode/ideal
  diode, before plugging USB while the battery is connected.
- A low-dropout regulator alone only works while the cell is above about 3.3 V plus its dropout.
- Always put a 100 µF to 470 µF capacitor near the ESP32 supply pin.

**Where the LED branch gets power.** From the battery/system rail (not from the 3.3 V logic rail) once the
regulation question is solved; during bench tests from USB 5 V (stable) or, at low current, from the board's 3V3.

**Power switch.** In series with the battery positive (or the OUT+ line), before everything else. It is a physical
switch, not a GPIO.

**Avoiding sag and resets while transmitting.**
1. LED current return straight to the star point; short, thick wires.
2. Bulk capacitor at the LED branch and another at the ESP32 supply.
3. Do not power the LED branch through the breadboard rail, or at least measure it.
4. Use a healthy cell: old or small cells have high internal resistance and sag under pulse current.
5. Measure the 3.3 V rail with an oscilloscope while sending; the ESP32 module is specified for 3.0 V to 3.6 V *(to verify
   in its datasheet)* and the firmware records the reset reason of every boot (*Herramientas IR > Historial arranques*;
   a `BROWNOUT` entry means the supply dipped).

### 4.8 Grounding and decoupling summary

- One star ground. USB GND, battery negative, MOSFET source, bulk capacitors and the ESP32 GND all meet there.
- 100 nF ceramic at each IC supply (OLED, TSOP) and 100 µF-470 µF bulk where power enters.
- Keep the TSOP and its wires away from the LED branch and from the display cable.

## 5. What not to do

- Do not drive the LED from a GPIO. (Current state: the tester asked for exactly that as a **temporary bench measure**
  on 2026-11-01 because only IRFZ44N parts were at hand, with 220 Ω to GND; see `docs/logs/2026-11-01-b8.md`. Fit a
  proper driver, a logic-level MOSFET or an NPN, before treating the device as finished.)
- Do not connect the LED branch to the battery before the current-limiting resistor and the gate pull-down exist.
- Do not leave a transmission test running unattended; the bench commands are bounded on purpose.
- Do not charge a cell while the device runs from the same TP4056 output (no power-path).
- Do not put 5 V on a 3.3 V pin, and do not rely on a datasheet number that you did not read for the part in your hands.

## Sources used

- Vishay, TSOP382.., TSOP384.. datasheet, Rev. 2.1, document 82491: <https://www.vishay.com/docs/82491/tsop382.pdf>
- XL6009 datasheet revisions and module notes (conflicting input ranges): components101 and XLSEMI mirrors found
  through search, e.g. <https://components101.com/regulators/xl6009-pwm-switching-regulator-buck-boost>
- TP4056 behaviour with a load: <https://www.best-microcontroller-projects.com/tp4056-page2.html> (hobbyist
  article; confirm against the charger IC datasheet)
- IRFZ44N / IRLZ44N figures: distributor pages, e.g. <https://components101.com/mosfets/irfz44n-datasheet-pinout-features>
  (read the manufacturer datasheet for the exact part you use)
