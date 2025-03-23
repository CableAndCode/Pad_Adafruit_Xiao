#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// ====== Adresy I2C dla gamepadów ======
constexpr uint8_t GAMEPAD1_ADDR = 0x50;
constexpr uint8_t GAMEPAD2_ADDR = 0x51;

// ====== Mapowanie przycisków ======
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

// ====== Dead zone dla joysticków ======
#define DEAD_ZONE 10 //jeszcze nie wykorzystywane


// ====== Zmienne globalne (extern) ======
// Używamy `extern`, żeby zmienne były tylko zadeklarowane w nagłówku, a faktycznie zdefiniowane w pliku .cpp
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

// ====== Konfiguracja ESP-NOW ======
extern esp_now_peer_info_t peerInfo;



#endif // PARAMETERS_H
