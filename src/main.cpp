#include <Arduino.h>
#include <Adafruit_seesaw.h>
#include <esp_now.h>
#include <WiFi.h>
#include "parameters.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <joystick_read.h>

// ----- Konfiguracja sprzętowa -----

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

// Stałe definiujące pozycje i rozmiary obszarów statystyk
constexpr int STAT_X_POS = 0;
constexpr int STAT_Y_FAILED_SEC = 0;
constexpr int STAT_Y_TOTAL_FAILED = 10;
constexpr int STAT_Y_TOTAL_SENT = 20;
constexpr int L_Button_pressed = 30;
constexpr int R_Button_pressed = 40;
constexpr int L_X_JOY_POS = 50;
constexpr int L_Y_JOY_POS = 60;
constexpr int R_X_JOY_POS = 70;
constexpr int R_Y_JOY_POS = 80;
constexpr int driftL_X_POS = 90;
constexpr int driftL_Y_POS = 100;
constexpr int driftR_X_POS = 110;
constexpr int driftR_Y_POS = 120;
constexpr int STAT_RECT_WIDTH = 128;
constexpr int STAT_RECT_HEIGHT = 20;

// Piny wyświetlacza TFT
#define TFT_CS     2
#define TFT_RST    3
#define TFT_DC     4
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

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

        // Normalizacja wartości – przykładowa transformacja
    
        /*
        int localL_Joystick_x = -localL_Joystick_raw_x + 512;
        int localL_Joystick_y = -localL_Joystick_raw_y + 512;
        int localR_Joystick_x = localR_Joystick_raw_x - 512;
        int localR_Joystick_y = localR_Joystick_raw_y - 512;

        int localL_Joystick_x_message = localL_Joystick_x - (512 - offsetL_X);
        int localL_Joystick_y_message = localL_Joystick_y - (512 - offsetL_Y);
        int localR_Joystick_x_message = localR_Joystick_x - (-512 + offsetR_X);
        int localR_Joystick_y_message = localR_Joystick_y - (-512 + offsetR_Y);
        */
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
    const TickType_t xFrequency = pdMS_TO_TICKS(50); // Wysyłka co 50 ms
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
// Wyświetla statystyki transmisji na TFT.
void vTaskESPNowStats(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // Aktualizacja co 100 ms
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

        // Aktualizacja tylko obszaru dla Failed/sec
        tft.fillRect(STAT_X_POS, STAT_Y_FAILED_SEC, STAT_RECT_WIDTH, STAT_RECT_HEIGHT, ST77XX_BLACK);
        tft.setCursor(STAT_X_POS, STAT_Y_FAILED_SEC);
        tft.setTextColor(ST77XX_WHITE);
        tft.setTextSize(1);
        tft.print("Failed/sec: ");
        tft.println(failedPerSecond);

        // Aktualizacja obszaru dla Total failed
        tft.fillRect(STAT_X_POS, STAT_Y_TOTAL_FAILED, STAT_RECT_WIDTH, STAT_RECT_HEIGHT, ST77XX_BLACK);
        tft.setCursor(STAT_X_POS, STAT_Y_TOTAL_FAILED);
        tft.print("Total failed: ");
        tft.println(failedMessages);

        // Aktualizacja obszaru dla Total sent
        tft.fillRect(STAT_X_POS, STAT_Y_TOTAL_SENT, STAT_RECT_WIDTH, STAT_RECT_HEIGHT, ST77XX_BLACK);
        tft.setCursor(STAT_X_POS, STAT_Y_TOTAL_SENT);
        tft.print("Total sent: ");
        tft.println(totalMessages);

        // Aktualizacja obszaru dla joysticków L i R
        tft.fillRect(STAT_X_POS, L_X_JOY_POS, STAT_RECT_WIDTH, STAT_RECT_HEIGHT, ST77XX_BLACK);
        tft.setCursor(STAT_X_POS, L_X_JOY_POS);
        tft.print("L_X: ");
        tft.println(message.L_Joystick_x_message);
        tft.fillRect(STAT_X_POS, L_Y_JOY_POS, STAT_RECT_WIDTH, STAT_RECT_HEIGHT, ST77XX_BLACK);
        tft.setCursor(STAT_X_POS, L_Y_JOY_POS);
        tft.print("L_Y: ");
        tft.println(message.L_Joystick_y_message);
        tft.fillRect(STAT_X_POS, R_X_JOY_POS, STAT_RECT_WIDTH, STAT_RECT_HEIGHT, ST77XX_BLACK);
        tft.setCursor(STAT_X_POS, R_X_JOY_POS);
        tft.print("R_X: ");
        tft.println(message.R_Joystick_x_message);
        tft.fillRect(STAT_X_POS, R_Y_JOY_POS, STAT_RECT_WIDTH, STAT_RECT_HEIGHT, ST77XX_BLACK);
        tft.setCursor(STAT_X_POS, R_Y_JOY_POS);
        tft.print("R_Y: ");
        tft.println(message.R_Joystick_y_message);  

        // Aktualizacja obszaru dla Button pressed
        tft.fillRect(STAT_X_POS, L_Button_pressed, STAT_RECT_WIDTH, STAT_RECT_HEIGHT, ST77XX_BLACK);
        tft.setCursor(STAT_X_POS, L_Button_pressed);
        tft.print("L: ");
                if (! (message.L_Joystick_buttons_message & (1UL << BUTTON_A))) {
                    tft.println("Button A pressed");
                }        
                if (! (message.L_Joystick_buttons_message & (1UL << BUTTON_B))) {
                    tft.println("Button B pressed");
                }        
                if (! (message.L_Joystick_buttons_message & (1UL << BUTTON_X))) {
                    tft.println("Button X pressed");
                }        
                if (! (message.L_Joystick_buttons_message & (1UL << BUTTON_Y))) {
                    tft.println("Button Y pressed");
                }        
                if (! (message.L_Joystick_buttons_message & (1UL << BUTTON_SELECT))) {
                    tft.println("Button SELECT pressed");
                }        
                if (! (message.L_Joystick_buttons_message & (1UL << BUTTON_START))) {
                    tft.println("Button START pressed");
                }
        tft.fillRect(STAT_X_POS, R_Button_pressed, STAT_RECT_WIDTH, STAT_RECT_HEIGHT, ST77XX_BLACK);
        tft.setCursor(STAT_X_POS, R_Button_pressed);
        tft.print("R: ");
                if (! (message.R_Joystick_buttons_message & (1UL << BUTTON_A))) {
                    tft.println("Button A pressed");
                }
                if (! (message.R_Joystick_buttons_message & (1UL << BUTTON_B))) {
                    tft.println("Button B pressed");
                }
                if (! (message.R_Joystick_buttons_message & (1UL << BUTTON_X))) {
                    tft.println("Button X pressed");
                }        
                if (! (message.R_Joystick_buttons_message & (1UL << BUTTON_Y))) {
                    tft.println("Button Y pressed");
                }        
                if (! (message.R_Joystick_buttons_message & (1UL << BUTTON_SELECT))) {
                    tft.println("Button SELECT pressed");
                }        
                if (! (message.R_Joystick_buttons_message & (1UL << BUTTON_START))) {
                    tft.println("Button START pressed");
                }
        tft.fillRect(STAT_X_POS, driftL_X_POS, STAT_RECT_WIDTH, STAT_RECT_HEIGHT, ST77XX_BLACK);
        tft.setCursor(STAT_X_POS, driftL_X_POS);
        tft.print("L_X offset: ");
        tft.println(offsetL_X);
        tft.fillRect(STAT_X_POS, driftL_Y_POS, STAT_RECT_WIDTH, STAT_RECT_HEIGHT, ST77XX_BLACK);
        tft.setCursor(STAT_X_POS, driftL_Y_POS);
        tft.print("L_Y offset: ");
        tft.println(offsetL_Y);
        tft.fillRect(STAT_X_POS, driftR_X_POS, STAT_RECT_WIDTH, STAT_RECT_HEIGHT, ST77XX_BLACK);
        tft.setCursor(STAT_X_POS, driftR_X_POS);
        tft.print("R_X offset: ");
        tft.println(offsetR_X);
        tft.fillRect(STAT_X_POS, driftR_Y_POS, STAT_RECT_WIDTH, STAT_RECT_HEIGHT, ST77XX_BLACK);
        tft.setCursor(STAT_X_POS, driftR_Y_POS);
        tft.print("R_Y offset: ");
        tft.println(offsetR_Y);                  
    }

}

void setup() {
    //setCpuFrequencyMhz(80); // Zmiana częstotliwości CPU na 80 MHz
    Serial.begin(115200);
    Wire.begin(5, 6);  // Konfiguracja I2C – SDA, SCL (dostosuj do swojego sprzętu)

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
    


    // Inicjalizacja TFT
    tft.initR(INITR_BLACKTAB);
    tft.fillScreen(ST77XX_BLACK);

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
