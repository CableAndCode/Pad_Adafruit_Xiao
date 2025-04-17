#include "parameters.h"

// ====== Mutex for protecting access to global message data ======
SemaphoreHandle_t messageMutex = NULL;

// ====== Global joystick offset and communication tracking variables ======
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



// ====== ESP-NOW peer configuration ======
esp_now_peer_info_t peerInfo;