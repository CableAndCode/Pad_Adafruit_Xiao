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
| TFT SCLK         | GPIO 10           | TFT Display          |
| TFT MOSI         | GPIO 11           | TFT Display          |
| TFT DC           | GPIO 7            | TFT Display          |
| TFT CS           | GPIO 8            | TFT Display          |
| TFT RESET        | GPIO 9            | TFT Display          |

---

### Libraries Used
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) – GPLv3
- [Adafruit seesaw library](https://github.com/adafruit/Adafruit_Seesaw) – MIT
- ESP32 Arduino Core (including ESP-NOW and WiFi) – Apache 2.0 / LGPL

---

### License
This project is licensed under the **GNU General Public License v3.0**. See the [LICENSE](LICENSE) file for full details.

---

## Road Map – Pad Controller (ESP-NOW, Xiao ESP32-S3)

### Stage 1 – Minimum Viable Product (MVP)
- [x] Concurrent execution of tasks using FreeRTOS
- [x] Reading raw values from analog joysticks
- [x] Dead zone handling, hysteresis, and scaling for joystick input
- [x] Serializing all gamepad data into a structured message
- [x] Transmitting data via ESP-NOW to the mecanum platform
- [x] Confirmed reception of data on the platform side (via OnDataSent callback)
- [x] Visualizing current joystick and button data on the TFT screen
- [x] Synchronization mechanisms to protect shared data using a FreeRTOS mutex

### Stage 2 – Diagnostics and Local Interface
- [ ] Remove or redesign the button visualization for minimal screen usage
- [ ] Display RSSI signal strength using `esp_now_get_peer_rssi()` on TFT
- [ ] Initial splash screen on startup showing system information and connection status
- [ ] Display Pad-side communication errors on the TFT
- [ ] Display Mecanum Platform errors on the TFT
- [ ] Display Monitor-side errors on the TFT (once monitor can send them)
- [ ] Display live parameters received from the Mecanum Platform

### Stage 3 – Power Supply and Ergonomics
- [ ] Migrate from breadboard to a soldered protoboard; add physical power switch
- [ ] Integrate battery monitoring and display battery level/status on TFT
- [ ] Implement deep sleep and wake-up logic (via time or button triggers)
- [ ] Add additional display screen(s) for alternate visualizations
- [ ] Support for all buttons: L_select, L_start, L_mode, LXY, LAB, RXY, RAB, R_select, R_start, R_mode
- [ ] Build a temporary or final enclosure for handheld use

### Stage 4 – Optional Extensions
- [ ] Enable ESP-NOW encryption for secure communication
- [ ] Add user-selectable control profiles (e.g., tuning curves, axis inversion, presets)

