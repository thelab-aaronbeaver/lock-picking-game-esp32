# Wiring Reference

This project uses an ESP32 with three analog cylinder inputs, three LEDs, and a single button. The LCD uses I2C.

## Pin map

| Function | ESP32 Pin | Notes |
| --- | --- | --- |
| Cylinder 1 (analog) | GPIO 32 | Analog input |
| Cylinder 2 (analog) | GPIO 35 | Analog input (input-only pin) |
| Cylinder 3 (analog) | GPIO 34 | Analog input (input-only pin) |
| LED 1 | GPIO 14 | Digital output |
| LED 2 | GPIO 12 | Digital output |
| LED 3 | GPIO 13 | Digital output |
| Start/Reset Button | GPIO 15 | `INPUT_PULLUP` (pressed = LOW) |
| LCD SDA | GPIO 21 | I2C SDA (default ESP32) |
| LCD SCL | GPIO 22 | I2C SCL (default ESP32) |

## Power and wiring notes

- **Button**: wire one side to GPIO 15 and the other to GND. No external pull-up required.
- **LEDs**: connect each LED with a series resistor (e.g., 220–330 Ω) to its GPIO pin.
- **Cylinder sensors**: connect each potentiometer:
  - One side to 3.3V
  - One side to GND
  - Wiper to the cylinder GPIO (32/35/34)
- **LCD**: connect to 5V or 3.3V depending on your module; ensure grounds are common.

## LCD I2C address

The code assumes `0x27`. If your module uses `0x3F`, update:

```
#define I2C_ADDR 0x27
```

## Important ESP32 notes

- GPIO 34 and 35 are input-only (fine for analog sensors).
- GPIO 15 is a strapping pin; avoid strong pull-ups/pull-downs that could affect boot. Using `INPUT_PULLUP` with a simple button to GND is generally OK.
