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

// Obiekty do obsługi gamepadów
Adafruit_seesaw ss1, ss2;

JoystickReader joystickReaderL(offsetL_X, offsetL_Y, true, true);
JoystickReader joystickReaderR(offsetR_X, offsetR_Y, false, false);


// Globalna struktura wiadomości z pada (heartbeat w polu timestamp)
Message_from_Pad message;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.println("📡 OnDataSent called!");

    if (memcmp(mac_addr, macMonitorDebug, 6) == 0) {
        if (status == ESP_NOW_SEND_SUCCESS) {
            Serial.print("✅ Monitor: OK ");
        } else {
            Serial.print("❌ Monitor: ERROR ");
        }
    }

    if (memcmp(mac_addr, macPlatformMecanum, 6) == 0) {
        if (status == ESP_NOW_SEND_SUCCESS) {
             Serial.print("✅ Platform: OK ");
        } else {
            Serial.print("❌ Platform: ERROR ");
        }
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
        unsigned long localTimestamp = millis();            // Odczyt czasu uzyty do heartbeatu
        
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
        totalMessages++;
        message.totalMessages = totalMessages;
        memcpy(&localMsg, &message, sizeof(Message_from_Pad));
        xSemaphoreGive(messageMutex);

        // Wysłanie danych przez ESP-NOW do monitora debug
        esp_err_t result = esp_now_send(macMonitorDebug, (uint8_t *)&localMsg, sizeof(Message_from_Pad));
        if (result == ESP_OK) ESP_NOW_Monitor_Error = false;
        else {
            ESP_NOW_Monitor_Error = true;
            ESP_NOW_Monitor_Send_Error_Counter++;
        }
       

        // Wysłanie danych przez ESP-NOW do platformy mecanum
        result = esp_now_send(macPlatformMecanum, (uint8_t *)&localMsg, sizeof(Message_from_Pad));
        if (result == ESP_OK) ESP_NOW_Platform_Error = false;
        else {
            ESP_NOW_Platform_Error = true;
            ESP_NOW_Platform_Send_Error_Counter++;
        }
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
        bool L_Button_A = !(message.L_Joystick_buttons_message & (1UL << BUTTON_A));
        bool L_Button_B = !(message.L_Joystick_buttons_message & (1UL << BUTTON_B));
        bool L_Button_X = !(message.L_Joystick_buttons_message & (1UL << BUTTON_X));
        bool L_Button_Y = !(message.L_Joystick_buttons_message & (1UL << BUTTON_Y));
        bool L_Button_SELECT = !(message.L_Joystick_buttons_message & (1UL << BUTTON_SELECT));
        bool L_Button_START = !(message.L_Joystick_buttons_message & (1UL << BUTTON_START));
        bool R_Button_A = !(message.R_Joystick_buttons_message & (1UL << BUTTON_A));
        bool R_Button_B = !(message.R_Joystick_buttons_message & (1UL << BUTTON_B));
        bool R_Button_X = !(message.R_Joystick_buttons_message & (1UL << BUTTON_X));
        bool R_Button_Y = !(message.R_Joystick_buttons_message & (1UL << BUTTON_Y));
        bool R_Button_SELECT = !(message.R_Joystick_buttons_message & (1UL << BUTTON_SELECT));
        bool R_Button_START = !(message.R_Joystick_buttons_message & (1UL << BUTTON_START));
        xSemaphoreGive(messageMutex);

        // Aktualizacja wyświetlacza
        display.updateJoystick(lx, ly, rx, ry);
        display.updateStatus(lx, ly, rx, ry, L_Button_A, L_Button_B, L_Button_X, L_Button_Y, L_Button_SELECT, L_Button_START, R_Button_A, R_Button_B, R_Button_X, R_Button_Y, R_Button_SELECT, R_Button_START);

        /* Wyświetl komunikat
        if (ESP_NOW_Monitor_Error || ESP_NOW_Platform_Error) {
            display.showMessage("ESP-NOW OK");
        } else { 
            display.showMessage("ESP-NOW ERROR");
        }
        */
       display.updateButtonsL(L_Button_A, L_Button_B, L_Button_X, L_Button_Y, L_Button_SELECT, L_Button_START);
       display.updateButtonsR(R_Button_A, R_Button_B, R_Button_X, R_Button_Y, R_Button_SELECT, R_Button_START);
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
    xTaskCreate(TaskGamepads, "Gamepads", 4096, NULL, 1, NULL);                 //task do odczytu joysticków
    xTaskCreate(TaskESPNow, "ESPNowSend", 4096, NULL, 1, NULL);                 //task do wysyłania danych przez ESP-NOW
    xTaskCreate(TaskTFTScreen, "TFTScreen", 4096, NULL, 1, NULL);               //task do wyświetlania danych na ekranie TFT
}

void loop() {
    // Pusta pętla – wszystkie operacje działają w taskach FreeRTOS
}
