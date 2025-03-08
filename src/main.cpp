#include <Arduino.h>
#include <Adafruit_seesaw.h>
#include <esp_now.h>
#include <WiFi.h>
#include "parameters.h"
#include <SPI.h>
#include <joystick_read.h>
#include <TFT_eSPI.h>
#include <DisplayManager.h>


// ----- Konfiguracja sprzętowa -----

//TFT Display
DisplayManager display;

// Adres MAC odbiornika – ustaw właściwy adres, do którego chcesz wysyłać dane
uint8_t receiverMAC[] = {0xA0, 0xB7, 0x65, 0x4B, 0xC5, 0x30}; 

// Adresy I2C dla gamepadów
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



// ----- Globalne zmienne i obiekty -----

//zmienne do przechowywania wartości dryftu joysticków


// Obiekty do obsługi gamepadów
Adafruit_seesaw ss1, ss2;

volatile int offsetL_X = 0;
volatile int offsetL_Y = 0;
volatile int offsetR_X = 0;
volatile int offsetR_Y = 0;
 

JoystickReader joystickReaderL(offsetL_X, offsetL_Y, true, true);
JoystickReader joystickReaderR(offsetR_X, offsetR_Y, false, false);


// Globalna struktura wiadomości z pada (heartbeat w polu timestamp)
Message_from_Pad message;

// Mutex do ochrony globalnej struktury (odczyt/zapis)
SemaphoreHandle_t messageMutex = NULL;

// Statystyki transmisji (opcjonalnie)
volatile uint32_t totalMessages = 0;
volatile uint32_t failedMessages = 0;
volatile uint32_t lastFailedCount = 0;
volatile uint32_t failedPerSecond = 0;

  


// Konfiguracja ESP-NOW
esp_now_peer_info_t peerInfo;

// ----- Callback wysyłania -----
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    totalMessages++;
    if (status != ESP_NOW_SEND_SUCCESS) {
        failedMessages++;
    }
}

// ----- TASK 1: Odczyt danych gamepadów (TaskGamepads) -----
void TaskGamepads(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(10);  // Odczyt co 10 ms
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        // Odczyt danych i normalizacja poza sekcją krytyczną
        unsigned long localTimestamp = millis();
        
        // Odczyt surowych wartości joysticków
        int localL_Joystick_raw_x = ss1.analogRead(14);
        int localL_Joystick_raw_y = ss1.analogRead(15);
        int localR_Joystick_raw_x = ss2.analogRead(14);
        int localR_Joystick_raw_y = ss2.analogRead(15);

       
        int localL_Joystick_x = joystickReaderL.getCorrectedValueX(localL_Joystick_raw_x);
        int localL_Joystick_y = joystickReaderL.getCorrectedValueY(localL_Joystick_raw_y);
        int localR_Joystick_x = joystickReaderR.getCorrectedValueX(localR_Joystick_raw_x);
        int localR_Joystick_y = joystickReaderR.getCorrectedValueY(localR_Joystick_raw_y);

        // Odczyt stanów przycisków
        int localL_Joystick_buttons_message = ss1.digitalReadBulk(button_mask);
        int localR_Joystick_buttons_message = ss2.digitalReadBulk(button_mask2);

        // Krótka sekcja krytyczna – kopiowanie lokalnych danych do globalnej struktury
        xSemaphoreTake(messageMutex, portMAX_DELAY);
        message.timestamp = localTimestamp;
        
        message.L_Joystick_raw_x = localL_Joystick_raw_x;
        message.L_Joystick_raw_y = localL_Joystick_raw_y;
        message.R_Joystick_raw_x = localR_Joystick_raw_x;
        message.R_Joystick_raw_y = localR_Joystick_raw_y;
        
        message.L_Joystick_x_message = localL_Joystick_x;
        message.L_Joystick_y_message = localL_Joystick_y;
        message.R_Joystick_x_message = localR_Joystick_x;
        message.R_Joystick_y_message = localR_Joystick_y;
        
        message.L_Joystick_buttons_message = localL_Joystick_buttons_message;
        message.R_Joystick_buttons_message = localR_Joystick_buttons_message;
        xSemaphoreGive(messageMutex);

        // Przykładowe monitorowanie zużycia stosu – wywołanie funkcji
        UBaseType_t freeStack = uxTaskGetStackHighWaterMark(NULL);
        Serial.print("Wolna pamięć stosu: ");
        Serial.println(freeStack);

        


        // Odczekanie do następnego cyklu
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// ----- TASK 2: Wysyłanie danych ESP-NOW (TaskESPNow) -----
// Co 50 ms odczytuje dane (z mutexem) i wysyła je przez ESP-NOW.
void TaskESPNow(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // Wysyłka co 20 ms
    TickType_t xLastWakeTime = xTaskGetTickCount();
    Message_from_Pad localMsg;
    
    while (1) {

        xSemaphoreTake(messageMutex, portMAX_DELAY);
        memcpy(&localMsg, &message, sizeof(Message_from_Pad));
        xSemaphoreGive(messageMutex);

        esp_err_t result = esp_now_send(receiverMAC, (uint8_t *)&localMsg, sizeof(Message_from_Pad));
        // Debug można odkomentować:
        // if (result == ESP_OK) Serial.println("📡 Dane wysłane");
        // else Serial.println("❌ Błąd wysyłania ESP-NOW");

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}


// ----- TASK 3: Wyświetlanie statystyk ESP-NOW (vTaskESPNowStats) -----
void vTaskESPNowStats(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(40); // Aktualizacja co 40 ms
    TickType_t xLastWakeTime = xTaskGetTickCount();
    TickType_t lastTimestamp = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        TickType_t currentTimestamp = xTaskGetTickCount();
        TickType_t elapsedTime = currentTimestamp - lastTimestamp;
        lastTimestamp = currentTimestamp;
        float secondsElapsed = elapsedTime / (float)configTICK_RATE_HZ;
        failedPerSecond = (failedMessages - lastFailedCount) / secondsElapsed;
        lastFailedCount = failedMessages;

        // Odczytaj wartości joysticków
        xSemaphoreTake(messageMutex, portMAX_DELAY);
        int lx = message.L_Joystick_x_message;
        int ly = message.L_Joystick_y_message;
        int rx = message.R_Joystick_x_message;
        int ry = message.R_Joystick_y_message;
        xSemaphoreGive(messageMutex);

        // Aktualizacja wyświetlacza
        display.updateJoystick(lx, ly, rx, ry);
        display.updateStatus(totalMessages, failedMessages);

        // Wyświetl komunikat
        if (failedMessages == 0) {
            display.showMessage("ESP-NOW OK");
        } else {
            display.showMessage("ESP-NOW ERROR");
        }
    }
}


void setup() {
    Serial.begin(115200);
    Wire.begin(5, 6);  // Konfiguracja I2C – SDA, SCL
    display.begin();

    

    // Inicjalizacja gamepadów
    if (!ss1.begin(GAMEPAD1_ADDR) || !ss2.begin(GAMEPAD2_ADDR)) {
        Serial.println("❌ Gamepad not found!");
        while (1) delay(100);
    }
    Serial.println("✅ Gamepad OK!");

    // Konfiguracja wejść przycisków
    ss1.pinModeBulk(button_mask, INPUT_PULLUP);
    ss1.setGPIOInterrupts(button_mask, 1);
    ss2.pinModeBulk(button_mask2, INPUT_PULLUP);
    ss2.setGPIOInterrupts(button_mask2, 1);
        #if defined(IRQ_PIN)
            pinMode(IRQ_PIN, INPUT);
        #endif
    
    //inicjalizacja joysticków, pobranie wartości początkowych offsetu
    offsetL_X = ss1.analogRead(14);
    offsetL_Y = ss1.analogRead(15);
    offsetR_X = ss2.analogRead(14);
    offsetR_Y = ss2.analogRead(15);
    joystickReaderL.setOffset(offsetL_X, offsetL_Y);
    joystickReaderR.setOffset(offsetR_X, offsetR_Y);
    


    // Konfiguracja ESP-NOW
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

    // Utworzenie mutexu do ochrony globalnej struktury danych
    messageMutex = xSemaphoreCreateMutex();
    if (messageMutex == NULL) {
        Serial.println("❌ Błąd tworzenia mutexu!");
        while (1) delay(100);
    }

    // Tworzenie tasków FreeRTOS
    xTaskCreate(TaskGamepads, "Gamepads", 4096, NULL, 1, NULL);
    xTaskCreate(TaskESPNow, "ESPNowSend", 4096, NULL, 1, NULL);
    xTaskCreate(vTaskESPNowStats, "ESPNowStats", 4096, NULL, 1, NULL);
}

void loop() {
    // Pusta pętla – wszystkie operacje działają w taskach FreeRTOS
}
