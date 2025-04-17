


Pad Controller is a custom ESP-NOW remote based on Xiao ESP32-S3, designed to send joystick and button input to a mecanum platform in real-time using FreeRTOS. It includes TFT visualization, synchronization with mutex, and future plans for diagnostics, power integration, and secure communication.


### Libraries Used
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) – GPLv3
- [Adafruit seesaw library](https://github.com/adafruit/Adafruit_Seesaw) – MIT
- Arduino Core for ESP32 – LGPL / Apache 2.0

### License
This project is licensed under the GNU General Public License v3.0 – see the [LICENSE](LICENSE) file for details.




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

