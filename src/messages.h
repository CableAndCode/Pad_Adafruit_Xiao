#ifndef MESSAGES_H
#define MESSAGES_H

#include <Arduino.h>

// Struktura wiadomości z pada (z domyślnymi inicjalizacjami, jeśli chcesz)
typedef struct Message_from_Pad {
    uint32_t timeStamp = 0;                     // Heartbeat – bieżący czas (millis())
    uint32_t messageSequenceNumber = 0;         // Liczba wysłanych wiadomości
    int16_t L_Joystick_x_message = 0;
    int16_t L_Joystick_y_message = 0;
    int16_t R_Joystick_x_message = 0;
    int16_t R_Joystick_y_message = 0;
    uint32_t L_Joystick_buttons_message = 0;
    uint32_t R_Joystick_buttons_message = 0;
    int16_t L_Joystick_raw_x = 0;
    int16_t L_Joystick_raw_y = 0;
    int16_t R_Joystick_raw_x = 0;
    int16_t R_Joystick_raw_y = 0;
} Message_from_Pad;

// Jeśli potrzebujesz też innych struktur, umieść je tutaj

#endif // MESSAGES_H
