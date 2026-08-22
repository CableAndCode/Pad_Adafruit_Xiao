## Pad Controller – Xiao ESP32-S3 + ESP-NOW + FreeRTOS

![Pad Controller Breadboard – Xiao ESP32-S3 + Seesaw](./images/esp-32_Adafruit_TFT.jpeg)
![Pad Controller Breadboard – Xiao ESP32-S3 + Seesaw Fritzing Diagram](./images/esp-32_Adafruit_TFT_Fritzing_Diagram.png)


*Prototype of the ESP-NOW based gamepad using two Adafruit Seesaw joysticks, Xiao ESP32-S3 and a 1.8" TFT SPI display.*


This custom controller is built on the Xiao ESP32-S3 and communicates via ESP-NOW. It reads two analog joysticks using Adafruit Seesaw modules (I2C) and displays live data on a 1.8" ST7735 TFT screen using the TFT_eSPI library. All key functions run under FreeRTOS, enabling smooth and concurrent execution.
The controller transmits structured control data to:
- A mecanum wheel robot platform, and
- A dedicated debug monitor ESP32 for telemetry and diagnostics.

---

### Components Used

| Part                         | Model / Type              | Notes                                |
|------------------------------|---------------------------|--------------------------------------|
| Main MCU                     | Seeed Studio Xiao ESP32-S3| ESP-NOW, FreeRTOS, I2C capable       |
| Joystick modules (x2)        | Adafruit Seesaw           | I2C interface, handles buttons too   |
| TFT display                  | 1.8" SPI TFT (ST7735)     | 128x160, works with TFT_eSPI         |
| Buttons                      | Built-in via Seesaw       | Read as digital GPIO bitmask         |
| Misc wiring                  | Dupont / soldered         | Breadboard-friendly prototyping      |

---

### Pin Connections

| Signal           | Xiao ESP32-S3 Pin | Connected Device     |
|------------------|-------------------|-----------------------|
| I2C SDA          | GPIO 5            | Both Seesaw modules  |
| I2C SCL          | GPIO 6            | Both Seesaw modules  |
| TFT SCLK         | GPIO 07           | TFT Display          |
| TFT MOSI         | GPIO 09           | TFT Display          |
| TFT DC           | GPIO 04           | TFT Display          |
| TFT CS           | GPIO 02           | TFT Display          |
| TFT RESET        | GPIO 03           | TFT Display          |

---

### Libraries Used
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) – GPLv3
- [Adafruit seesaw library](https://github.com/adafruit/Adafruit_Seesaw) – MIT
- ESP32 Arduino Core (including ESP-NOW and WiFi) – Apache 2.0 / LGPL

---

---
### Build & Flash

Requires [PlatformIO](https://platformio.org/).

```bash
git clone https://github.com/CableAndCode/Pad_Adafruit_Xiao.git
cd Pad_Adafruit_Xiao
pio run -t upload
```

**Display configuration.** No manual editing of the TFT_eSPI library is
required. All display settings (driver, pins, SPI speed, fonts) are passed
as `build_flags` in `platformio.ini` and are applied automatically.

**MAC addresses.** The project builds out of the box using the placeholder
addresses in `src/mac_addresses.h`. Edit that file with the MAC addresses of
your own devices, or create `src/mac_addresses_private.h` — if present, it
takes precedence and is excluded from version control.

A warning about `TOUCH_CS` during the build is expected; the ST7735 panel
used here has no touch layer.
---

### License
Released under the MIT License. See [LICENSE](LICENSE).

Third-party libraries retain their own licenses:
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) – FreeBSD (BSD 2-clause)
- [Adafruit seesaw](https://github.com/adafruit/Adafruit_Seesaw) – MIT
- [Adafruit BusIO](https://github.com/adafruit/Adafruit_BusIO) – MIT
- [Adafruit ST7735/ST7789](https://github.com/adafruit/Adafruit-ST7735-Library) – BSD
- ESP32 Arduino Core – Apache 2.0 / LGPL

---

