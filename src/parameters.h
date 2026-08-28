#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// ====== I2C addresses of the two gamepads ======
constexpr uint8_t GAMEPAD1_ADDR = 0x50;
constexpr uint8_t GAMEPAD2_ADDR = 0x51;

// ====== Button-to-seesaw-pin mapping ======
// The numbering is sparse, which is why the frame carries a densely packed
// bitmask instead (see BTN_* in messages.h).
constexpr uint8_t BUTTON_X      = 6;
constexpr uint8_t BUTTON_Y      = 2;
constexpr uint8_t BUTTON_A      = 5;
constexpr uint8_t BUTTON_B      = 1;
constexpr uint8_t BUTTON_SELECT = 0;
constexpr uint8_t BUTTON_START  = 16;

const uint32_t button_mask = (1UL << BUTTON_X) | (1UL << BUTTON_Y) |
                             (1UL << BUTTON_START) | (1UL << BUTTON_A) |
                             (1UL << BUTTON_B) | (1UL << BUTTON_SELECT);
// Both gamepads are the same model, so they share the mask.
const uint32_t button_mask2 = button_mask;

// ====== Globals (defined in parameters.cpp) ======
// Joystick centre readings, taken once at boot.
extern volatile int offsetL_X;
extern volatile int offsetL_Y;
extern volatile int offsetR_X;
extern volatile int offsetR_Y;

// Guards the shared outgoing message structure.
extern SemaphoreHandle_t messageMutex;

// Frames sent so far; doubles as the sequence number in MSG_PAD_CONTROL.
extern volatile uint32_t totalMessages;

// ====== ESP-NOW ======
extern esp_now_peer_info_t peerInfo;

#endif // PARAMETERS_H
