#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <SPI.h>
#include <Adafruit_seesaw.h>
#include <TFT_eSPI.h>

#include "parameters.h"
#include "messages.h"
#include "mac_addresses.h"
#include "errors.h"
#include "joystick_read.h"
#include "DisplayManager.h"


// --- Hardware Configuration ---

DisplayManager display; // TFT display manager
Adafruit_seesaw ss1, ss2; // I2C-based gamepad controllers

JoystickReader joystickReaderL(offsetL_X, offsetL_Y, true, true);
JoystickReader joystickReaderR(offsetR_X, offsetR_Y, false, false);

Message_from_Pad message; // Global message structure from pad

// --- Callback: Confirm delivery status of sent ESP-NOW messages ---
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (memcmp(mac_addr, macMonitorDebug, 6) == 0) {
        ESP_NOW_Monitor_Error = (status != ESP_NOW_SEND_SUCCESS);
        if (ESP_NOW_Monitor_Error) {
            ESP_NOW_Monitor_Send_Error_Counter++;
        }
    }

    if (memcmp(mac_addr, macPlatformMecanum, 6) == 0) {
        ESP_NOW_Platform_Error = (status != ESP_NOW_SEND_SUCCESS);
        if (ESP_NOW_Platform_Error) {
            ESP_NOW_Platform_Send_Error_Counter++;
        }
    }
}




// --- TASK 1: Reading gamepad input (TaskGamepads) ---
// Every 20 ms, reads and normalizes joystick values and updates the global message structure.
// Mutex-protected access ensures safe sharing of data.
void TaskGamepads(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // 50 Hz
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        unsigned long localTimeStamp = millis();

        // Read raw joystick values
        int localL_Joystick_raw_x = ss1.analogRead(14);
        int localL_Joystick_raw_y = ss1.analogRead(15);
        int localR_Joystick_raw_x = ss2.analogRead(14);
        int localR_Joystick_raw_y = ss2.analogRead(15);

        // Normalize joystick values using offset correction
        int localL_Joystick_x = joystickReaderL.getCorrectedValueX(localL_Joystick_raw_x);
        int localL_Joystick_y = joystickReaderL.getCorrectedValueY(localL_Joystick_raw_y);
        int localR_Joystick_x = joystickReaderR.getCorrectedValueX(localR_Joystick_raw_x);
        int localR_Joystick_y = joystickReaderR.getCorrectedValueY(localR_Joystick_raw_y);

        // Read button states
        int localL_Joystick_buttons_message = ss1.digitalReadBulk(button_mask);
        int localR_Joystick_buttons_message = ss2.digitalReadBulk(button_mask2);

        // Critical section: update global message structure
        xSemaphoreTake(messageMutex, portMAX_DELAY);
        message.timeStamp = localTimeStamp;

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

// --- TASK 2: Sending data via ESP-NOW (TaskESPNow) ---
// Every 50 ms, copies the latest gamepad data and transmits it via ESP-NOW to monitor and platform receivers.
void TaskESPNow(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(50); // 20 Hz
    TickType_t xLastWakeTime = xTaskGetTickCount();
    Message_from_Pad localMsg;

    while (1) {
        // Copy shared data (protected by mutex)
        xSemaphoreTake(messageMutex, portMAX_DELAY);
        totalMessages++;
        message.messageSequenceNumber = totalMessages;
        memcpy(&localMsg, &message, sizeof(Message_from_Pad));
        xSemaphoreGive(messageMutex);

        // Send to debug monitor
        esp_now_send(macMonitorDebug, (uint8_t *)&localMsg, sizeof(Message_from_Pad));

        // Send to mecanum platform
        esp_now_send(macPlatformMecanum, (uint8_t *)&localMsg, sizeof(Message_from_Pad));

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}


// --- TASK 3: Updating the TFT display (TaskTFTScreen) ---
// Every 50 ms, reads joystick and button states and updates the display accordingly.
void TaskTFTScreen(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(50); // 20 Hz
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // Read current values from shared message (protected)
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

        // Update display with latest joystick positions and button states
        display.updateJoystick(lx, ly, rx, ry);
        display.updateButtonsL(L_Button_A, L_Button_B, L_Button_X, L_Button_Y, L_Button_SELECT, L_Button_START);
        display.updateButtonsR(R_Button_A, R_Button_B, R_Button_X, R_Button_Y, R_Button_SELECT, R_Button_START);
    }
}

// --- Setup function ---
// Initializes all hardware and software components before entering task loop.
void setup() {
    Serial.begin(115200);
    Wire.begin(5, 6); // I2C setup: SDA = 5, SCL = 6
    display.begin();

    // Initialize gamepads
    if (!ss1.begin(GAMEPAD1_ADDR) || !ss2.begin(GAMEPAD2_ADDR)) {
        Serial.println("❌ Gamepad not found!");
        while (1) delay(100);
    }
    Serial.println("✅ Gamepad OK!");

    // Configure input pins for buttons
    ss1.pinModeBulk(button_mask, INPUT_PULLUP);
    ss1.setGPIOInterrupts(button_mask, 1);
    ss2.pinModeBulk(button_mask2, INPUT_PULLUP);
    ss2.setGPIOInterrupts(button_mask2, 1);
#ifdef IRQ_PIN
    pinMode(IRQ_PIN, INPUT);
#endif

    // Read initial joystick offsets
    offsetL_X = ss1.analogRead(14);
    offsetL_Y = ss1.analogRead(15);
    offsetR_X = ss2.analogRead(14);
    offsetR_Y = ss2.analogRead(15);
    joystickReaderL.setOffset(offsetL_X, offsetL_Y);
    joystickReaderR.setOffset(offsetR_X, offsetR_Y);

    // Initialize ESP-NOW communication
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW Init Failed");
        return;
    }
    esp_now_register_send_cb(OnDataSent);

    // Add debug monitor peer
    memcpy(peerInfo.peer_addr, macMonitorDebug, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("❌ Failed to add peer (monitor)");
        return;
    }

    // Add mecanum platform peer
    memcpy(peerInfo.peer_addr, macPlatformMecanum, 6);
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("❌ Failed to add peer (platform)");
        return;
    }

    // Create mutex to protect shared message structure
    messageMutex = xSemaphoreCreateMutex();
    if (messageMutex == NULL) {
        Serial.println("❌ Failed to create mutex!");
        while (1) delay(100);
    }

    // Create FreeRTOS tasks
    xTaskCreate(TaskGamepads, "Gamepads", 2048, NULL, 1, NULL);
    xTaskCreate(TaskESPNow, "ESPNowSend", 2048, NULL, 1, NULL);
    xTaskCreate(TaskTFTScreen, "TFTScreen", 4096, NULL, 1, NULL);
}

// --- Main loop ---
// Empty loop, all logic is handled in FreeRTOS tasks.
void loop() {
    // Nothing to do here
}