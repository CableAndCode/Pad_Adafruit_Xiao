## Pad Controller – Xiao ESP32-S3 + ESP-NOW + FreeRTOS

![Pad Controller Breadboard – Xiao ESP32-S3 + Seesaw](./esp-32_Adafruit_TFT.jpeg)

*Prototype of the ESP-NOW based gamepad using two Adafruit Seesaw joysticks, Xiao ESP32-S3 and a 1.8" TFT SPI display.*


This is a custom ESP-NOW-based gamepad controller built using the Xiao ESP32-S3. It reads two analog joysticks via I2C (using Adafruit seesaw), normalizes and serializes the data into a structured message, and transmits it wirelessly via ESP-NOW to two independent recipients:
- a mecanum robot platform (in development), and
- a dedicated debug monitor ESP32 for telemetry and diagnostics.

Future development includes diagnostics, power monitoring, enclosure, and optional security features.

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
### TFT_eSPI Configuration (User_Setup.h)

For correct operation with the Xiao ESP32-S3 and the 1.8" ST7735 SPI TFT display, make sure your User_Setup.h in the TFT_eSPI library includes the following (or equivalent) definitions:

#define ST7735_DRIVER

#define TFT_WIDTH 128

#define TFT_HEIGHT 160

#define ST7735_REDTAB

#define TFT_MOSI 9

#define TFT_SCLK 7

#define TFT_CS 2

#define TFT_DC 4

#define TFT_RST 3

#define USE_HSPI_PORT

#define SPI_FREQUENCY 27000000

#define SPI_READ_FREQUENCY 20000000

#define SPI_TOUCH_FREQUENCY 2500000

#define TFT_RGB_ORDER TFT_RGB

#define LOAD_GLCD

#define LOAD_FONT2

#define LOAD_FONT4

#define LOAD_FONT6

#define LOAD_FONT7

#define LOAD_FONT8

#define LOAD_GFXFF

#define SMOOTH_FONT

---

### License
This project is licensed under the **GNU General Public License v3.0**. See the [LICENSE](LICENSE) file for full details.

---

