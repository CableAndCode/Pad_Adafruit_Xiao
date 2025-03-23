#include "parameters.h"

// ====== Semaphore do ochrony dostępu do zmiennych globalnych ======

SemaphoreHandle_t messageMutex = NULL;

// ====== Definicja zmiennych globalnych ======
volatile int offsetL_X = 0;
volatile int offsetL_Y = 0;
volatile int offsetR_X = 0;
volatile int offsetR_Y = 0;
volatile uint32_t totalMessages = 0;
volatile uint32_t failedMessages = 0;
volatile uint32_t lastFailedCount = 0;
volatile uint32_t failedPerSecond = 0;
volatile int consecutiveFailures = 0;
volatile int espNowStatus = 0;

//volatile TickType_t lastHeartbeatTimeMonitor = 0;
//volatile TickType_t lastHeartbeatTimePlatform = 0;

// ====== Konfiguracja ESP-NOW ======
esp_now_peer_info_t peerInfo;
