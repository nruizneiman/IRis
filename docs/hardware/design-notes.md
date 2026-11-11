# Design notes

Why the circuit is the way it is, and how to size it for parts that differ from the author's. The circuit that was
actually built is in [wiring](wiring.md); start there.

These notes were written before the build, as a careful plan, and then corrected with what the build showed. Facts
marked *(datasheet)* were read in the manufacturer document named next to them. Facts marked *(to verify)* are
assumptions to check before relying on them.

What the build changed from the plan:

- **Emitter driver.** The plan used a MOSFET. The IRFZ44N at hand did not switch on from a 3.3 V GPIO, so the unit
  uses an NPN transistor (2N2222).
- **LED supply.** The plan fed the LEDs from the battery rail. The unit feeds them from the 5 V rail of the boost
  converter.
- **Regulation.** Of the options below, the unit uses a boost converter to 5 V into the VIN pin, and it is switched
  off while it charges.

## What to identify before wiring

Parts bought as generic modules are not always what the listing says. For each one, this is what matters and how to
find out. The blocks (B1, B3...) are those of the [bring-up plan](bring-up.md).

| Part | What is usually known | What to find out | How |
|---|---|---|---|
| ESP32-WROOM-32 dev board, 30 pins | Module family | The exact pin labels of your board | Compare the silkscreen with the [pin map](wiring.md#pin-map) (B2) |
| OLED 1.3", 128x64, I2C | Size, interface | **Controller: SH1106 or SSD1306**, and the I2C address | `display_probe` (B3) |
| TSOP38238 | 38 kHz demodulating receiver | Whether the part is a genuine TSOP38238, and its pin order | Marking and behaviour (B5) |
| IR LEDs | Wavelength | **Model, forward voltage, current and pulse ratings** | Markings, datasheet, measurement (B1, B7) |
| Resistors | Nominal value | The real value | Measure out of circuit (B1) |
| Driver transistor | Type | Whether it switches on fully from 3.3 V | Measure the drop at the real current (B6) |
| 18650 cell | Nominal 3.6 / 3.7 V | Protected or not, capacity, condition | Markings, voltage (B1, B9) |
| USB-C charger module | "With protection" in the listing | The chips, and whether it has a power path | Chip markings (B1, B9) |
| Boost module "XL6009" | Looks like an XL6009 | The exact chip and its **minimum input voltage** | Markings and a bench test (B9) |

## IR receiver (TSOP38238)

*(Vishay datasheet TSOP382.., TSOP384.., Rev. 2.1, document 82491.)*

- Pinning: **1 = OUT, 2 = GND, 3 = VS.** Use the package drawing in the datasheet to find pin 1. Do not rely on
  memory or on the silkscreen order of a breakout board.
- Supply voltage 2.0 V to 5.5 V, absolute maximum 6 V. Supply current about 0.35 mA at 3.3 V. Output current 5 mA at
  most.
- The output is active low and has an **internal 30 kΩ pull-up**, so it connects directly to a GPIO of the ESP32 when
  the receiver is powered from 3.3 V.
- The datasheet recommends a resistor and a capacitor in series with VS "in case there are strong ripple or spikes on
  the supply line". This guide gives no values because they were not readable in the text used. A 100 nF capacitor
  close to the receiver is harmless; add the filter if captures show noise while the emitter or the display is active.
  The author's unit has none.

Placement: facing outwards, away from the emitter LEDs and shielded from their light. Otherwise the receiver is
blinded by the transmissions of the device itself. The firmware also ignores captures during a transmission and just
after it.

Behaviour to know *(datasheet)*: this version, with gain control tuned for long bursts, needs at least 10 carrier
cycles per burst, a gap of at least 12 cycles after short bursts and a longer gap after long bursts, and it suppresses
continuous signals. NEC and RC5 / RC6 are listed as supported. For Sony 12 / 15 / 20-bit codes the datasheet points
to a different part (TSOP38S40), so **Sony captures may be less reliable with a TSOP38238**. The mark and space
durations measured through it are approximations of the transmitted ones.

## IR emitter stage

A GPIO of the ESP32 cannot power an IR LED: a transistor switches the LED current.

### The general circuit

```
 LED supply (+)  -----------+-----------------+
                            |                 |
                        [C_bulk]          [R_led]            one resistor per LED
                            |                 |
                            |              LED anode
                            |              LED cathode
                            |                 |
                            |         drain / collector
 GPIO23 --[R_series]--+------------ gate / base            source / emitter ---+
                      |                                                        |
                   [R_pulldown]                                                |
                      |                                                        |
 GND -----------------+--------------------------------------------------------+
```

- **Series resistor.** For an NPN it sets the base current: 1 kΩ from 3.3 V gives about (3.3 - 0.7) / 1 kΩ = 2.6 mA
  *(calculated)*, enough for tens of milliamps of collector current with a gain of 100 or more. For a MOSFET, 100 Ω to
  220 Ω limits the gate charging current and ringing.
- **Pull-down resistor**, about 100 kΩ. It keeps the transistor off while the ESP32 is in reset or booting, when the
  GPIO floats. It must be present before the LED is ever powered.
- **Bulk capacitor**, 100 µF or more, close to the LED branch. The carrier switches on and off every 26 µs (38 kHz).
  A current of I amps for the on-time t needs roughly C = I·t / dV for a droop of dV: with the default duty of one
  third, about 8.7 µs of on-time per period, 0.1 A and 50 mV that is about 17 µF *(an estimate, not measured)*. It does
  not help against the slow sag of a weak battery over a 10 ms burst.
- A freewheel diode is not needed: the load is not inductive.
- Return the LED current straight to the common ground, not through the thin wire that feeds the ESP32.

### Choosing the transistor

The ESP32 drives the control pin to 3.3 V at most.

- **NPN (2N2222, BC337).** Switches on fully from 3.3 V through a base resistor. This is what the author's unit uses.
- **MOSFET.** Needs a part whose datasheet specifies Rds(on) at a gate voltage of 3.3 V or less, or whose maximum gate
  threshold is clearly below 3.3 V and whose transfer curve shows several times the LED current at 3.3 V.
  - **IRFZ44N** *(datasheet values reported by distributors)*: gate threshold 2 V to 4 V, Rds(on) of 17.5 mΩ
    **specified at VGS = 10 V**. Two units were tried on the author's bench and neither switched on from 3.3 V.
  - **IRLZ44N**, the logic-level sibling: threshold about 1 to 2 V, Rds(on) quoted at 10 V and 5 V in the sources
    found. A 3.3 V figure was not found, so check the transfer curve in its datasheet.
  - Other small logic-level MOSFETs (AO3400A, IRLML6344) were not checked here: read their datasheets first.

### LED resistor

For a continuous current I through the LED while the transistor is on:

```
R_led = (V_supply - Vf - V_drop) / I_target        P_R = I^2 * R_led   (at 100 % duty; the real duty is lower)
```

- `V_supply` is the highest voltage the branch will see: 5 V from the boost converter or from USB.
- `Vf` is the forward voltage of the LED **at your current**. The diode mode of a multimeter applies only about 1 mA,
  so it reads lower than the Vf at tens of milliamps. Using that lower Vf in the formula gives a larger resistor and a
  lower real current, which is the safe direction for first tests.
- `V_drop` is the drop across the transistor: about 0.2 V for a saturated NPN, tens of millivolts for a good MOSFET.
- Size the resistor as if the LED stayed on all the time, even though the average current during a 38 kHz burst is
  about one third of it. A software bug must not be able to overload the LED.
- While the rating of the LED is unknown, stay at or below about 20 mA. Raise the current only after finding the
  datasheet of the LED.

Worked example, the author's unit: V_supply = 5 V, Vf assumed 1.4 V, V_drop assumed 0.2 V, R = 100 Ω gives
(5 - 1.4 - 0.2) / 100 = 34 mA per LED. The TSAL6200 is rated for 100 mA continuous. Neither Vf nor the current was
measured.

### More LEDs

- Give **each LED its own resistor** and put the branches in parallel. Never share one resistor between parallel LEDs:
  their forward voltages differ and so would their currents.
- One transistor can drive all the branches if the **total** current is within its rating. Otherwise build a second
  driver on GPIO19, which is reserved for it. The firmware accepts one channel only today.
- Measure the supply sag again each time a branch is added. More LEDs draw more current from a battery with internal
  resistance, and that is what causes resets.

## Battery divider

```
battery (+) --[R]--+--[R]-- GND          GPIO33 --- midpoint, with 100 nF to GND
```

Two equal resistors halve the battery voltage: at most about 2.1 V with a full cell, inside the range of the ADC.

- With 2 x 100 kΩ the divider draws about 21 µA; with 2 x 10 kΩ, as on the author's unit, about 210 µA, against the
  roughly 80 mA of the running board. Take it from the **switched** side, after the power switch, so that it never
  drains the cell while the unit is off.
- The firmware reads the pin only when built with `-DIRIS_BATTERY_RATIO_PERMILLE=<ratio x 1000>` (2000 for two equal
  resistors). Without the flag the screen shows `Bat n/a`, because a floating pin would show noise.
- The ADC of the ESP32 is not linear. Compare the reading with a multimeter (bench command `a` prints the millivolts at
  the pin). How the level is estimated and shown is in the [interface notes](../firmware/ui.md#battery).

## Battery, charging and regulation

Three jobs that must not be confused: **charging the cell**, **protecting the cell** and **making the supply
voltage**.

**Charger module with protection (TP4056 class, with a DW01 and 8205 pair).**

- Connect the **cell to B+ / B-** and the **load to OUT+ / OUT-**, the protected output. A load wired to B+ / B-
  bypasses the over-current and over-discharge protection.
- A charger of this class ends the charge by watching the current fall. A load connected while charging disturbs that
  measurement, so the charge may never end; the protection chip does not fix this. Therefore **do not run the device
  from the module while it charges** unless you add a real power-path stage.
- Options, simplest first:
  1. **Device off while charging.** The power switch disconnects the load; charge only with the switch off. This is
     what the author's unit does.
  2. **A charger module with power path** (load sharing) stated in its datasheet. The MCP73871 is one charger IC of
     this kind mentioned in the sources consulted; no specific module was verified.
  3. **A discrete power path**: a P-channel MOSFET that disconnects the battery from the load when USB is present, plus
     a Schottky diode on the USB side. A concept only; it needs its own validation.

**Making the supply from one cell (3.0 V to 4.2 V).** The cell goes below the 3.3 V that the board needs, so a
regulator that only steps down is not enough over the whole range.

- *Boost to 5 V into the VIN pin.* The regulator of the board then makes 3.3 V. This is what the author's unit does,
  with an XL6009 module. The minimum input of the XL6009 is **not settled**: one datasheet revision lists 3.6 V to 36 V
  and a newer one 5 V to 32 V, and some module vendors say it does not work below 5 V. On the author's unit it runs
  from the cell (4.96 V on VIN with the cell at 3.87 V); the voltage at which it stops was not measured. Test yours
  with a resistive load before connecting the ESP32.
- *Buck-boost to 3.3 V into the 3V3 pin.* Works across the cell range, but then 3V3 must **not** be fed by USB at the
  same time: the regulator of the board and the module would fight.
- A low-dropout regulator alone only works while the cell is above about 3.3 V plus its dropout.

**Power switch.** A physical switch in series with the battery positive, before the converter. It is not a GPIO.

**Avoiding sag and resets while transmitting.**

1. Return the LED current straight to the common ground, with short, thick wires.
2. Put a bulk capacitor at the LED branch and near the supply pin of the ESP32.
3. Do not power the LED branch through a breadboard rail: its contact resistance shows up as voltage drop.
4. Use a healthy cell. Old or small cells have a high internal resistance and sag under pulse current.
5. The firmware records the reset reason of every boot (*Settings > Diagnostics > Boot history*). A `BROWNOUT` entry
   means the supply dipped.

## Grounding and decoupling

- One common ground. The battery negative, the converter, the transistor, the bulk capacitors and the ESP32 GND all
  meet there.
- Keep the receiver and its wires away from the LED branch and from the display cable.

## What not to do

- Do not drive an LED from a GPIO.
- Do not power the LED branch before its resistor and the pull-down of the transistor are fitted.
- Do not leave a transmission test running unattended; the bench commands are bounded on purpose.
- Do not charge a cell while the device runs from the same charger output, unless it has a power path.
- Do not put 5 V on a 3.3 V pin, and do not rely on a datasheet number that you did not read for the part in your
  hands.

## Sources

- Vishay, TSOP382.., TSOP384.. datasheet, Rev. 2.1, document 82491: <https://www.vishay.com/docs/82491/tsop382.pdf>
- XL6009 datasheet revisions and module notes, with conflicting input ranges: components101 and XLSEMI mirrors, for
  example <https://components101.com/regulators/xl6009-pwm-switching-regulator-buck-boost>
- TP4056 behaviour with a load: <https://www.best-microcontroller-projects.com/tp4056-page2.html> (a hobbyist article;
  confirm against the datasheet of the charger IC)
- IRFZ44N and IRLZ44N figures: distributor pages, for example
  <https://components101.com/mosfets/irfz44n-datasheet-pinout-features> (read the manufacturer datasheet for the exact
  part you use)
