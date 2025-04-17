#ifndef MESSAGES_H
#define MESSAGES_H

#include <Arduino.h>

// Structure representing a message sent from the gamepad
// Contains joystick positions, button states, and raw values

typedef struct Message_from_Pad {
    uint32_t timeStamp = 0;                     // Heartbeat – current time (millis())
    uint32_t messageSequenceNumber = 0;         // Number of messages sent (sequence counter)

    int16_t L_Joystick_x_message = 0;           // Normalized values
    int16_t L_Joystick_y_message = 0;
    int16_t R_Joystick_x_message = 0;
    int16_t R_Joystick_y_message = 0;

    uint32_t L_Joystick_buttons_message = 0;    // Button bitfield (left gamepad)
    uint32_t R_Joystick_buttons_message = 0;    // Button bitfield (right gamepad)

    int16_t L_Joystick_raw_x = 0;               // Raw ADC readings
    int16_t L_Joystick_raw_y = 0;
    int16_t R_Joystick_raw_x = 0;
    int16_t R_Joystick_raw_y = 0;
} Message_from_Pad;


#endif // MESSAGES_H
