# Adafruit Mini I2C STEMMA QT Gamepad (PID 5743)

A hardware cheat sheet for the two gamepads used in the pad. Compiled from
Adafruit's documentation on 2026-08-26, so nothing here has to be guessed or
looked up twice. Sources at the end.

## Seesaw pin numbering

The seesaw controller (an ATtiny817) exposes its own pin numbers — these are
**not** ESP32 pins, and they are not contiguous:

| function | seesaw pin |
|---|---|
| button A | 5 |
| button B | 1 |
| button X | 6 |
| button Y | 2 |
| SELECT | 0 |
| START | 16 |
| joystick X axis | 14 (`analogRead`) |
| joystick Y axis | 15 (`analogRead`) |

Six buttons each, so two gamepads mean **twelve** states to transmit. The
numbers are sparse (0, 1, 2, 5, 6, 16) — the highest bit is 16, which is why the
code holds them in a `uint32_t`. For transmission they are worth repacking
densely: twelve bits fit in a `uint16_t` with four to spare.

## Levels and ranges

- The buttons are **active low**: configured `INPUT_PULLUP`, pressed = bit 0.
  Hence the negation in the code: `!(buttons & (1UL << BUTTON_A))`.
- Joystick: a **10-bit** ADC, range `0..1023`. Adafruit's example inverts the
  axis (`1023 - analogRead(14)`) so it matches the stick's orientation.
- Power: the same voltage as the microcontroller's logic — 3.3 V here.

## I2C addresses

`0x50` by default. Address jumpers (cut the trace):

| jumpers | address |
|---|---|
| none (as shipped) | 0x50 |
| A0 cut | 0x51 |
| A1 cut | 0x52 |
| A0 and A1 cut | 0x53 |

In this pad: left `GAMEPAD1_ADDR = 0x50`, right `GAMEPAD2_ADDR = 0x51` (so A0 is
cut on the right-hand one). Bus: `Wire.begin(5, 6)` — SDA 5, SCL 6.

## What our code does not do, and Adafruit's example does

The example uses `ss.getVersion()` to check that the device at the address is
really product ID **5743**. Our `setup()` only checks that `begin()` succeeded,
which would also pass for a different seesaw device at the same address. A cheap
guard against mixing up boards.

## Sources

- https://learn.adafruit.com/gamepad-qt/pinouts
- https://learn.adafruit.com/gamepad-qt/arduino
- https://github.com/adafruit/Adafruit_Seesaw/blob/master/examples/Mini_I2C_Gamepad_QT/Mini_I2C_Gamepad_QT.ino
