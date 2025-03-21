#include <Arduino.h>
#include <Adafruit_seesaw.h>
#include <esp_now.h>
#include <WiFi.h>
#include "parameters.h"
#include "messages.h"
#include "mac_adresses.h"
#include "errors.h"
#include <SPI.h>
#include <joystick_read.h>
#include <TFT_eSPI.h>
#include <DisplayManager.h>


// ----- Konfiguracja sprzętowa -----

//TFT Display
DisplayManager display;


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

// Statystyki transmisji (opcjonalnie) i czasy
volatile uint32_t totalMessages = 0;
volatile uint32_t failedMessages = 0;
volatile uint32_t lastFailedCount = 0;
volatile uint32_t failedPerSecond = 0;
volatile int consecutiveFailures = 0;       // Licznik niepowodzeń esp-now
volatile int espNowStatus = 0;              // 0 = OK, 1 = WARNING, 2 = ERROR

//zmienne do sprawdzania heartbeatu osobne dla kadego peera (monitora i platformy), w przyszłości kolejne peery
volatile TickType_t lastHeartbeatTimeMonitor =0;
volatile TickType_t lastHeartbeatTimePlatform =0;    
  


// Konfiguracja ESP-NOW
esp_now_peer_info_t peerInfo;

// ----- Callback wysyłania -----
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    totalMessages++;
    if (status == ESP_NOW_SEND_SUCCESS) {
        failedMessages = 0;
    } else {
        failedMessages++;
    }
}

// ----- TASK 1: Odczyt danych gamepadów (TaskGamepads) -----
// Co 25 ms odczytuje dane z gamepadów, normalizuje je i zapisuje do globalnej struktury.
// Ochrona danych za pomocą mutexu.
void TaskGamepads(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(25);  // Odczyt 40 razy na sekundę
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        // Odczyt danych i normalizacja poza sekcją krytyczną
        //unsigned long localTimestamp = millis();            // Odczyt czasu uzyty do heartbeatu, stara wersja
        
        // Odczyt surowych wartości joysticków
        int localL_Joystick_raw_x = ss1.analogRead(14);
        int localL_Joystick_raw_y = ss1.analogRead(15);
        int localR_Joystick_raw_x = ss2.analogRead(14);
        int localR_Joystick_raw_y = ss2.analogRead(15);

        //odczyt wartości joysticków z uwzględnieniem dryftu
        int localL_Joystick_x = joystickReaderL.getCorrectedValueX(localL_Joystick_raw_x); 
        int localL_Joystick_y = joystickReaderL.getCorrectedValueY(localL_Joystick_raw_y);
        int localR_Joystick_x = joystickReaderR.getCorrectedValueX(localR_Joystick_raw_x);
        int localR_Joystick_y = joystickReaderR.getCorrectedValueY(localR_Joystick_raw_y);

        // Odczyt stanów przycisków
        int localL_Joystick_buttons_message = ss1.digitalReadBulk(button_mask);
        int localR_Joystick_buttons_message = ss2.digitalReadBulk(button_mask2);

        // Krótka sekcja krytyczna – kopiowanie lokalnych danych do globalnej struktury
        xSemaphoreTake(messageMutex, portMAX_DELAY);
        //message.timestamp = localTimestamp;
        
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

        // Przykładowe monitorowanie zużycia zasobow – można odkomentować
        /*
        UBaseType_t freeStack = uxTaskGetStackHighWaterMark(NULL); // Dostępna pamięć stosu dla tasku
        Serial.print("Wolna pamięć stosu: ");
        Serial.println(freeStack);

        Serial.print("Free heap: ");                 // Dostępna pamięć heap (RAM)
        Serial.println(esp_get_free_heap_size());

        Serial.print("Free PSRAM: ");                // Dostępna pamięć PSRAM (dodatkowa pamięć RAM)
        Serial.println(ESP.getFreePsram());

        uint32_t highWaterMark = uxTaskGetStackHighWaterMark(NULL); //Obciązenie CPU
        Serial.print("CPU Load: ");
        Serial.println(highWaterMark);
        */
        // Odczekanie do następnego cyklu
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// ----- TASK 2: Wysyłanie danych ESP-NOW (TaskESPNow) -----
// Co 40 ms odczytuje dane (z mutexem) i wysyła je przez ESP-NOW.
void TaskESPNow(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(40); // Wysyłka 25 razy na sekundę
    TickType_t xLastWakeTime = xTaskGetTickCount();
    Message_from_Pad localMsg;
    
    while (1) {

        xSemaphoreTake(messageMutex, portMAX_DELAY);
        memcpy(&localMsg, &message, sizeof(Message_from_Pad));
        xSemaphoreGive(messageMutex);

        // Wysłanie danych przez ESP-NOW do monitora debug
        esp_err_t result = esp_now_send(macMonitorDebug, (uint8_t *)&localMsg, sizeof(Message_from_Pad));
        // Debug można odkomentować:
        // if (result == ESP_OK) Serial.println("📡 Dane wysłane");
        // else Serial.println("❌ Błąd wysyłania ESP-NOW");

        // Wysłanie danych przez ESP-NOW do platformy mecanum
        result = esp_now_send(macPlatformMecanum, (uint8_t *)&localMsg, sizeof(Message_from_Pad));
        // Debug można odkomentować:
        // if (result == ESP_OK) Serial.println("📡 Dane wysłane");
        // else Serial.println("❌ Błąd wysyłania ESP-NOW");

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}


// ----- TASK 3: Wyświetlanie danych na wyswietlaczu TFT -----
// Co 40 ms aktualizuje wyświetlacz TFT z informacjami o statystykach ESP-NOW.
void TaskTFTScreen(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(40); // Aktualizacja co 40 ms
    TickType_t xLastWakeTime = xTaskGetTickCount();
    TickType_t lastTimestamp = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

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
            display.showMessage("ESP-NOW ErrrrRROR");
        }
    }
}

//-----------------Task 4: Sprawdzanie heartbeatu-----------//
// Celem jest sprawdzenie, czy dane z gamepadów są nadal przesyłane do platformy mecanum oraz do monitora debug
// Jeśli nie, celem jest wyświetlenie tej wiadomoci na wyświetlaczu TFT w tasku TaskTFTScreen
// zmiana nazewnictwa tasku vTaskESPNowStats na TaskTFTScreen
void TaskCheckHeartbeat(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(50); // Sprawdzanie co 50 ms
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        volatile TickType_t currentTime = xTaskGetTickCount();

        // Sprawdź, czy różnica w czasie przekroczyła limit
        if ((currentTime - lastHeartbeatTimeMonitor) > pdMS_TO_TICKS(500)) {
            Serial.println("⚠️ Brak heartbeatu Monitora!");
            // Możesz dodać np. restart ESP-NOW, zmianę kanału itd.
        }
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
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


    // Dodanie odbiorcy monitora debug
    memcpy(peerInfo.peer_addr, macMonitorDebug, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("❌ Failed to add peer");
        return;
    }

    // Dodanie odbiorcy platformy mecanum
    memcpy(peerInfo.peer_addr, macPlatformMecanum, 6);
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
    xTaskCreate(TaskGamepads, "Gamepads", 4096, NULL, 1, NULL);                 //task do odczytu joysticków
    xTaskCreate(TaskESPNow, "ESPNowSend", 4096, NULL, 1, NULL);                 //task do wysyłania danych przez ESP-NOW
    xTaskCreate(TaskTFTScreen, "TFTScreen", 4096, NULL, 1, NULL);               //task do statystyk
    xTaskCreate(TaskCheckHeartbeat, "Heartbeat", 4096, NULL, 1, NULL);          //task do sprawdzania heartbeatu
}

void loop() {
    // Pusta pętla – wszystkie operacje działają w taskach FreeRTOS
}
