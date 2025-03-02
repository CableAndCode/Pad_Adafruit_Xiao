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

// Gamepad – adresy I2C
constexpr uint8_t GAMEPAD1_ADDR = 0x50;
constexpr uint8_t GAMEPAD2_ADDR = 0x51;

// Mapowanie przycisków
constexpr uint8_t BUTTON_X      = 6;
constexpr uint8_t BUTTON_Y      = 2;
constexpr uint8_t BUTTON_A      = 5;
constexpr uint8_t BUTTON_B      = 1;
constexpr uint8_t BUTTON_SELECT = 0;
constexpr uint8_t BUTTON_START  = 16;
const uint32_t button_mask = (1UL << BUTTON_X) | (1UL << BUTTON_Y) | (1UL << BUTTON_START) |
                             (1UL << BUTTON_A) | (1UL << BUTTON_B) | (1UL << BUTTON_SELECT);
const uint32_t button_mask2 = button_mask;

// Definicje pinów wyświetlacza
constexpr uint8_t TFT_CS  = 2;
constexpr uint8_t TFT_RST = 3;
constexpr uint8_t TFT_DC  = 4;

// Obiekt wyświetlacza (dostosuj do swojego modelu)
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Zmienne globalne do liczenia wiadomości
volatile uint32_t totalMessages = 0;
volatile uint32_t failedMessages = 0;
volatile uint32_t failedPerSecond = 0;
volatile uint32_t lastFailedCount = 0;

// Muteks do synchronizacji statystyk (nie do samej struktury wiadomości)
SemaphoreHandle_t xMutex = NULL;

// Obiekty do obsługi gamepadów
Adafruit_seesaw ss1, ss2;

// Zamiast globalnej struktury message, użyjemy kolejki do przesyłania kopii
QueueHandle_t messageQueue = NULL;

// Numer sekwencyjny wiadomości
uint16_t sequenceNumber = 0;

// Konfiguracja ESP-NOW
esp_now_peer_info_t peerInfo;

// Callback po wysłaniu danych ESP-NOW
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    xSemaphoreTake(xMutex, portMAX_DELAY);
    totalMessages++;
    if (status != ESP_NOW_SEND_SUCCESS) {
        failedMessages++;
    }
    xSemaphoreGive(xMutex);
}

// Deklaracje tasków FreeRTOS
void TaskGamepads(void *pvParameters);
void TaskESPNow(void *pvParameters);
void TaskESPNowStats(void *pvParameters);

void setup() {
    Serial.begin(115200);
    Wire.begin(5, 6);  // SDA, SCL

    // Inicjalizacja gamepadów
    if (!ss1.begin(GAMEPAD1_ADDR) || !ss2.begin(GAMEPAD2_ADDR)) {
        Serial.println("❌ Gamepad not found!");
        while (1) { delay(100); }
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
    if (xMutex == NULL) {
        Serial.println("❌ Failed to create mutex");
        while (1) { delay(100); }
    }

    // Tworzenie kolejki – długość 1, zawsze najnowsza wartość
    messageQueue = xQueueCreate(1, sizeof(Message_from_Pad));
    if (messageQueue == NULL) {
        Serial.println("❌ Failed to create message queue");
        while (1) { delay(100); }
    }

    // Tworzenie tasków
    xTaskCreate(TaskGamepads, "Gamepads", 4096, NULL, 1, NULL);
    xTaskCreate(TaskESPNow, "ESPNowSend", 4096, NULL, 1, NULL);
    xTaskCreate(TaskESPNowStats, "ESPNowStats", 4096, NULL, 1, NULL);
}

void loop() {
    // Pętla główna pozostaje pusta – taski działają w tle.
}

// 🔥 TASK: Odczyt danych z gamepadów i przesyłanie ich do kolejki
void TaskGamepads(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(10);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    Message_from_Pad msg;  // lokalna kopia wiadomości

    for (;;) {
        msg.seqNum = sequenceNumber++;

        // Odczyt surowych wartości joysticków
        msg.L_Joystick_raw_x = ss1.analogRead(14);
        msg.L_Joystick_raw_y = ss1.analogRead(15);
        msg.R_Joystick_raw_x = ss2.analogRead(14);
        msg.R_Joystick_raw_y = ss2.analogRead(15);

        // Normalizacja wartości (-512 do 512)
        msg.L_Joystick_x_message = -msg.L_Joystick_raw_x + 512;
        msg.L_Joystick_y_message = -msg.L_Joystick_raw_y + 512;
        msg.R_Joystick_x_message = msg.R_Joystick_raw_x - 512;
        msg.R_Joystick_y_message = msg.R_Joystick_raw_y - 512;

        // Odczyt przycisków – jeśli zdefiniowano IRQ_PIN, pomijamy tę iterację
        #if defined(IRQ_PIN)
            if (!digitalRead(IRQ_PIN)) {
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }
        #endif
        msg.L_Joystick_buttons_message = ss1.digitalReadBulk(button_mask);
        msg.R_Joystick_buttons_message = ss2.digitalReadBulk(button_mask2);

        // Nadpisujemy poprzednią wartość w kolejce – zawsze trzymamy najnowszy stan
        xQueueOverwrite(messageQueue, &msg);

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// 🔥 TASK: Wysyłanie danych ESP-NOW z kolejki
void TaskESPNow(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(50);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    Message_from_Pad msg;

    for (;;) {
        // Pobierz najnowszą wiadomość z kolejki (bez usuwania) – jeżeli nic nie ma, opuść ten cykl
        if (xQueuePeek(messageQueue, &msg, 0) == pdTRUE) {
            esp_now_send(receiverMAC, (uint8_t *)&msg, sizeof(Message_from_Pad));
        }
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// 🔥 TASK: Wyświetlanie statystyk ESP-NOW
void TaskESPNowStats(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(100);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    TickType_t lastTimestamp = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        TickType_t currentTimestamp = xTaskGetTickCount();
        TickType_t elapsedTime = currentTimestamp - lastTimestamp;
        lastTimestamp = currentTimestamp;
        float secondsElapsed = elapsedTime / (float)configTICK_RATE_HZ;

        xSemaphoreTake(xMutex, portMAX_DELAY);
        failedPerSecond = (failedMessages - lastFailedCount) / secondsElapsed;
        lastFailedCount = failedMessages;
        xSemaphoreGive(xMutex);

        // Aktualizacja wyświetlacza – czyścimy cały ekran; w praktyce można odświeżać tylko zmienione fragmenty
        tft.fillRect(X_POS, Y_POS_FAILED_SEC, RECT_WIDTH, RECT_HEIGHT, ST77XX_BLACK);
        tft.setCursor(X_POS, Y_POS_FAILED_SEC);
        tft.print("Failed/sec: ");
        tft.println((float)failedPerSecond);

        // Czyszczenie tylko obszaru "Total failed"
        tft.fillRect(X_POS, Y_POS_TOTAL_FAILED, RECT_WIDTH, RECT_HEIGHT, ST77XX_BLACK);
        tft.setCursor(X_POS, Y_POS_TOTAL_FAILED);
        tft.print("Total failed: ");
        tft.println(failedMessages);

        // Czyszczenie tylko obszaru "Total sent"
        tft.fillRect(X_POS, Y_POS_TOTAL_SENT, RECT_WIDTH, RECT_HEIGHT, ST77XX_BLACK);
        tft.setCursor(X_POS, Y_POS_TOTAL_SENT);
        tft.print("Total sent: ");
        tft.println(totalMessages);
    }
}
