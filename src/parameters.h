#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <Arduino.h>

// Struktura wiadomości z pada – zamiast sekwencyjnego numeru mamy timestamp (heartbeat)
typedef struct Message_from_Pad {
    uint32_t timestamp;  // Heartbeat – bieżący czas (millis())
    int16_t L_Joystick_x_message;
    int16_t L_Joystick_y_message;
    int16_t R_Joystick_x_message;
    int16_t R_Joystick_y_message;
    uint32_t L_Joystick_buttons_message;
    uint32_t R_Joystick_buttons_message;
    int16_t L_Joystick_raw_x;
    int16_t L_Joystick_raw_y;
    int16_t R_Joystick_raw_x;
    int16_t R_Joystick_raw_y;
} Message_from_Pad;

// Struktura wiadomości z platformy mecanum – analogicznie
typedef struct Message_from_Platform_Mecanum {
    uint32_t timestamp;
    int16_t frontLeftSpeed;
    int16_t frontRightSpeed;
    int16_t rearLeftSpeed;
    int16_t rearRightSpeed;
    float pitch;
    float roll;
    float yaw;
    float batteryVoltage;
} Message_from_Platform_Mecanum;

// MAC adresy urządzeń – dostosuj do swojego sprzętu
uint8_t macFireBeetle[]         = {0xEC, 0x62, 0x60, 0x5A, 0x6E, 0xFC};                  
uint8_t macPlatformMecanum[]    = {0xDC, 0xDA, 0x0C, 0x55, 0xD5, 0xB8};      
uint8_t macModulXiao[]          = {0x34, 0x85, 0x18, 0x9E, 0x87, 0xD4};            
uint8_t macMonitorDebug[]       = {0xA0, 0xB7, 0x65, 0x4B, 0xC5, 0x30};          

#endif // PARAMETERS_H
