#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// ====== I2C addresses for gamepads ======
constexpr uint8_t GAMEPAD1_ADDR = 0x50;
constexpr uint8_t GAMEPAD2_ADDR = 0x51;

// ====== Button mapping ======
constexpr uint8_t BUTTON_X      = 6;
constexpr uint8_t BUTTON_Y      = 2;
constexpr uint8_t BUTTON_A      = 5;
constexpr uint8_t BUTTON_B      = 1;
constexpr uint8_t BUTTON_SELECT = 0;
constexpr uint8_t BUTTON_START  = 16;

const uint32_t button_mask = (1UL << BUTTON_X) | (1UL << BUTTON_Y) |
                             (1UL << BUTTON_START) | (1UL << BUTTON_A) |
                             (1UL << BUTTON_B) | (1UL << BUTTON_SELECT);
const uint32_t button_mask2 = button_mask;

// ====== Dead zone for joysticks ======
#define DEAD_ZONE 10

// ====== Global variables (extern) ======
extern volatile int offsetL_X;
extern volatile int offsetL_Y;
extern volatile int offsetR_X;
extern volatile int offsetR_Y;

extern SemaphoreHandle_t messageMutex;

extern volatile uint32_t totalMessages;
extern volatile uint32_t failedMessages;
extern volatile uint32_t lastFailedCount;
extern volatile uint32_t failedPerSecond;
extern volatile int consecutiveFailures;
extern volatile int espNowStatus;

extern volatile TickType_t lastHeartbeatTimeMonitor;
extern volatile TickType_t lastHeartbeatTimePlatform;

// ====== ESP-NOW configuration ======
extern esp_now_peer_info_t peerInfo;

#endif // PARAMETERS_H
