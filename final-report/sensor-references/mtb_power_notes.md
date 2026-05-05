# MTB Power Rail Notes

## Power Rails on the Motherboard
- **VB (~12V)** — raw battery voltage. Comes from LiPo battery (EC3 connector) or benchtop power supply (J5 screw terminal).
- **5V** — regulated from VB by U2 (L7805). Powers Teensy, op-amps, hex buffer.
- **3.3V** — regulated from VB by U3 (LD1086). Powers GPS, SD card, IMU.

## Key Finding: 3.3V and 5V are NOT on the Protoboard Connector

The protoboard connector (J1) and the labeled through-holes on the MTB (A#, E#, etc.) only carry:
- Signal pins (A0-A20, D24, D25, D32, D33, etc.)
- GND
- SCL / SDA (I2C)
- VB (raw battery voltage)

**No 5V or 3.3V lines go to J1 or the protoboard.**

## How to Get 3.3V on the Protoboard
1. Solder a wire from **test point TP2 or TP5** (both are 3.3V) on the MTB
2. Tap the **Teensy's 3.3V pin** directly

## How to Get 5V on the Protoboard
1. Tap the **Teensy's 5V pin** directly
2. Solder from the 5V regulator output or a 5V node on the MTB

## USB vs Battery Power
- USB from laptop provides **5V only** (no 12V)
- **VB (~12V)** only comes from the battery or benchtop supply
- BAT LOW LED (D5) lights up when VB drops below ~11.6V
