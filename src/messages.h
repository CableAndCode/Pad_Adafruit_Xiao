#ifndef MESSAGE_H
#define MESSAGE_H


#include <Arduino.h>

// Struktura wiadomości z pada 
typedef struct Message_from_Pad {
    uint32_t timestamp = 0;  // Heartbeat – bieżący czas (millis())
    uint32_t totalMessages = 0; // Liczba wysłanych wiadomości
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

/* Struktura wiadomości z platformy mecanum – analogicznie tutaj do niczego jeszcze nie potrzebna
typedef struct Message_from_Platform_Mecanum {
    uint32_t timestamp; // Heartbeat – bieżący czas (millis()) zobaczymy, czy się przyjmie, na razie pracujemy nad tym
    int16_t frontLeftSpeed;
    int16_t frontRightSpeed;
    int16_t rearLeftSpeed;
    int16_t rearRightSpeed;
    float pitch;
    float roll;
    float yaw;
    float batteryVoltage;
} Message_from_Platform_Mecanum;
*/

#endif // MESSAGE_H
