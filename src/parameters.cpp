#include "parameters.h"

// ====== Mutex protecting the shared message structure ======
SemaphoreHandle_t messageMutex = NULL;

// ====== Joystick offsets and the transmit counter ======
volatile int offsetL_X = 0;
volatile int offsetL_Y = 0;
volatile int offsetR_X = 0;
volatile int offsetR_Y = 0;

volatile uint32_t totalMessages = 0;

// ====== ESP-NOW peer configuration ======
esp_now_peer_info_t peerInfo;
