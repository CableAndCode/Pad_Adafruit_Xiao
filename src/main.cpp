#include <Arduino.h>
#include <Adafruit_seesaw.h>
#include <esp_now.h>
#include <WiFi.h>
#include "parameters.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// Adres MAC odbiornika ESP32 Serial Monitor (Debug)
uint8_t receiverMAC[] = {0xA0, 0xB7, 0x65, 0x4B, 0xC5, 0x30}; 

// Gamepad Adresy I2C
#define GAMEPAD1_ADDR 0x50
#define GAMEPAD2_ADDR 0x51


// Mapowanie przycisków
#define BUTTON_X         6
#define BUTTON_Y         2
#define BUTTON_A         5
#define BUTTON_B         1
#define BUTTON_SELECT    0
#define BUTTON_START    16
uint32_t button_mask = (1UL << BUTTON_X) | (1UL << BUTTON_Y) | (1UL << BUTTON_START) |
                       (1UL << BUTTON_A) | (1UL << BUTTON_B) | (1UL << BUTTON_SELECT);

uint32_t button_mask2 = (1UL << BUTTON_X) | (1UL << BUTTON_Y) | (1UL << BUTTON_START) |
                       (1UL << BUTTON_A) | (1UL << BUTTON_B) | (1UL << BUTTON_SELECT);

// Definicje pinów wyświetlacza
#define TFT_CS     2
#define TFT_RST    3
#define TFT_DC     4

// Obiekt wyświetlacza (dostosuj do swojego modelu)
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Zmienne globalne do liczenia wiadomości
volatile uint32_t totalMessages = 0;
volatile uint32_t failedMessages = 0;
volatile uint32_t failedPerSecond = 0;
volatile uint32_t lastFailedCount = 0;

// Muteks dla synchronizacji dostępu do zmiennych
SemaphoreHandle_t xMutex;

// Obiekty do obsługi gamepadów
Adafruit_seesaw ss1, ss2;

// Struktura wiadomości
Message_from_Pad message;

// Numer sekwencyjny wiadomości
uint16_t sequenceNumber = 0;

// ESP-NOW konfiguracja
esp_now_peer_info_t peerInfo;

// Callback po wysłaniu danych
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    xSemaphoreTake(xMutex, portMAX_DELAY);
    totalMessages++;
    if (status != ESP_NOW_SEND_SUCCESS) {
        failedMessages++;
    }
    xSemaphoreGive(xMutex);
}


// Taski FreeRTOS
void TaskGamepads(void *pvParameters);
void TaskESPNow(void *pvParameters);
void vTaskESPNowStats(void *pvParameters);

void setup() {
    Serial.begin(115200);
    Wire.begin(5, 6);  // SDA, SCL
    

    if (!ss1.begin(GAMEPAD1_ADDR) || !ss2.begin(GAMEPAD2_ADDR)) {
        Serial.println("❌ Gamepad not found!");
        while (1) delay(100);
    }

    Serial.println("✅ Gamepad OK!");

    ss1.pinModeBulk(button_mask, INPUT_PULLUP);
    ss1.setGPIOInterrupts(button_mask, 1);
    ss2.pinModeBulk(button_mask2, INPUT_PULLUP);
    ss2.setGPIOInterrupts(button_mask2, 1);
        #if defined(IRQ_PIN)
            pinMode(IRQ_PIN, INPUT);
        #endif

    // Inicjalizacja ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
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

    // Inicjalizacja wyświetlacza
    tft.initR(INITR_BLACKTAB);
    tft.fillScreen(ST77XX_BLACK);

    // Inicjalizacja muteksu
    xMutex = xSemaphoreCreateMutex();


    // Tworzymy taski
    xTaskCreate(TaskGamepads, "Gamepads", 4096, NULL, 1, NULL);
    xTaskCreate(TaskESPNow, "ESPNowSend", 4096, NULL, 1, NULL);
    xTaskCreate(vTaskESPNowStats, "ESPNowStats", 4096, NULL, 1, NULL);
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
        message.L_Joystick_x_message = - message.L_Joystick_raw_x + 512;
        message.L_Joystick_y_message = - message.L_Joystick_raw_y + 512;
        message.R_Joystick_x_message = message.R_Joystick_raw_x - 512;
        message.R_Joystick_y_message = message.R_Joystick_raw_y - 512;

        // Odczyt przycisków
        #if defined(IRQ_PIN)
            if(!digitalRead(IRQ_PIN)) {
             return;
                }
        #endif

    message.L_Joystick_buttons_message = ss1.digitalReadBulk(button_mask);
    message.R_Joystick_buttons_message = ss2.digitalReadBulk(button_mask2);


        vTaskDelay(pdMS_TO_TICKS(10));  // Odczyt co 10ms
    }
}

// 🔥 TASK: Wysyłanie ESP-NOW
void TaskESPNow(void *pvParameters) {
    while (1) {
        TickType_t xLastWakeTime = xTaskGetTickCount();
        const TickType_t xFrequency = pdMS_TO_TICKS(50);
        esp_err_t result = esp_now_send(receiverMAC, (uint8_t *)&message, sizeof(Message_from_Pad));
/*
        if (result == ESP_OK) {
            //Serial.println("📡 ESP-NOW Data Sent");
        } else {
            Serial.println("❌ ESP-NOW Send Failed");
        }
*/
        //vTaskDelay(pdMS_TO_TICKS(50));  // Wysyłanie co 50ms
    }
}

// 🔥 TASK: Statystyki ESP-NOW
void vTaskESPNowStats(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    TickType_t lastTimestamp = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100);

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

           // Sprawdzenie rzeczywistego czasu, który minął
        TickType_t currentTimestamp = xTaskGetTickCount();
        TickType_t elapsedTime = currentTimestamp - lastTimestamp;
        lastTimestamp = currentTimestamp;  // Aktualizacja znacznika czasu

    float secondsElapsed = elapsedTime / (float)configTICK_RATE_HZ;  // Konwersja ticków na sekundy

    xSemaphoreTake(xMutex, portMAX_DELAY);
    failedPerSecond = (failedMessages - lastFailedCount) / secondsElapsed;
    lastFailedCount = failedMessages;
    xSemaphoreGive(xMutex);


        // Wyczyść ekran i wyświetl nowe wartości
        tft.fillScreen(ST77XX_BLACK);
        tft.setCursor(10, 10);
        tft.setTextColor(ST77XX_WHITE);
        tft.setTextSize(1);

        tft.print("Failed/sec: ");
        tft.println((float)failedPerSecond);
        
        tft.print("Total failed: ");
        tft.println(failedMessages);
        
        tft.print("Total sent: ");
        tft.println(totalMessages);
    }
}
