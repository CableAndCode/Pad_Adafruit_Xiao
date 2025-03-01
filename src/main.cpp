#include <Arduino.h>
#include <Adafruit_seesaw.h>
#include <esp_now.h>
#include <WiFi.h>
#include "parameters.h"

// Adres MAC odbiornika ESP32 Serial Monitor (Debug)
uint8_t receiverMAC[] = {0xA0, 0xB7, 0x65, 0x4B, 0xC5, 0x30}; 

// Gamepad Adresy I2C
#define GAMEPAD1_ADDR 0x50
#define GAMEPAD2_ADDR 0x51

// Definicje bitów dla 6 przycisków na joystick
#define BUTTON_1 0
#define BUTTON_2 1
#define BUTTON_3 2
#define BUTTON_4 3
#define BUTTON_5 4
#define BUTTON_6 5

Adafruit_seesaw ss1, ss2;


Message_from_Pad message;
uint16_t sequenceNumber = 0;

// ESP-NOW konfiguracja
esp_now_peer_info_t peerInfo;

// Callback na potwierdzenie wysyłki
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("ESP-NOW Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// Taski FreeRTOS
void TaskGamepads(void *pvParameters);
void TaskESPNow(void *pvParameters);

void setup() {
    Serial.begin(115200);
    Wire.begin(5, 6);  // SDA, SCL

    if (!ss1.begin(GAMEPAD1_ADDR) || !ss2.begin(GAMEPAD2_ADDR)) {
        Serial.println("❌ Gamepad not found!");
        while (1) delay(100);
    }

    Serial.println("✅ Gamepad OK!");

    // Inicjalizacja ESP-NOW
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW Init Failed");
        return;
    }
    esp_now_register_send_cb(OnDataSent);

    memcpy(peerInfo.peer_addr, receiverMAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("❌ Failed to add peer");
        return;
    }

    // Tworzymy taski
    xTaskCreate(TaskGamepads, "Gamepads", 4096, NULL, 1, NULL);
    xTaskCreate(TaskESPNow, "ESPNowSend", 4096, NULL, 1, NULL);
}

void loop() {}

// 🔥 TASK: Pobieranie danych z gamepadów
void TaskGamepads(void *pvParameters) {
    while (1) {
        message.seqNum = sequenceNumber++;

        // Odczyt joysticków
        message.L_Joystick_raw_x = ss1.analogRead(14);
        message.L_Joystick_raw_y = ss1.analogRead(15);
        message.R_Joystick_raw_x = ss2.analogRead(14);
        message.R_Joystick_raw_y = ss2.analogRead(15);

        // Normalizacja (-512 do 512)
        message.L_Joystick_x_message = message.L_Joystick_raw_x - 512;
        message.L_Joystick_y_message = message.L_Joystick_raw_y - 512;
        message.R_Joystick_x_message = message.R_Joystick_raw_x - 512;
        message.R_Joystick_y_message = message.R_Joystick_raw_y - 512;

        // Odczyt przycisków
        uint32_t buttonsL_raw = ss1.digitalReadBulk(0xFFFF);
        uint32_t buttonsR_raw = ss2.digitalReadBulk(0xFFFF);


        Serial.printf("Raw Buttons (L): %08X  (R): %08X\n", buttonsL_raw, buttonsR_raw);

        message.L_Joystick_buttons_message = ~ss1.digitalReadBulk(0xFFFF) & 0b111111;
        message.R_Joystick_buttons_message = ~ss2.digitalReadBulk(0xFFFF) & 0b111111;

        vTaskDelay(pdMS_TO_TICKS(10));  // Odczyt co 10ms
    }
}

// 🔥 TASK: Wysyłanie ESP-NOW
void TaskESPNow(void *pvParameters) {
    while (1) {
        esp_err_t result = esp_now_send(receiverMAC, (uint8_t *)&message, sizeof(Message_from_Pad));

        if (result == ESP_OK) {
            //Serial.println("📡 ESP-NOW Data Sent");
        } else {
            Serial.println("❌ ESP-NOW Send Failed");
        }

        vTaskDelay(pdMS_TO_TICKS(50));  // Wysyłanie co 50ms
    }
}
