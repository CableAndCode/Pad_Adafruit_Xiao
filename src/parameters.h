#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <Arduino.h>

// Struktura wiadomości z gamepada
typedef struct Message_from_Pad {
    uint16_t seqNum = 0;
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

// Struktura wiadomości z platformy mecanum
typedef struct Message_from_Platform_Mecanum {
    uint16_t seqNum = 0;
    int16_t frontLeftSpeed = 0;
    int16_t frontRightSpeed = 0;
    int16_t rearLeftSpeed = 0;
    int16_t rearRightSpeed = 0;
    float pitch = 0;
    float roll = 0;
    float yaw = 0;
    float batteryVoltage = 0;
} Message_from_Platform_Mecanum;

// MAC adresy urządzeń
const uint8_t macFireBeetle[]      = {0xEC, 0x62, 0x60, 0x5A, 0x6E, 0xFC}; // FireBeetle ESP32-E
const uint8_t macPlatformMecanum[] = {0xDC, 0xDA, 0x0C, 0x55, 0xD5, 0xB8}; // Platforma mecanum z ESP32 S3 DEVKIT C-1 N8R2
const uint8_t macModulXiao[]       = {0x34, 0x85, 0x18, 0x9E, 0x87, 0xD4};   // Seeduino Xiao ESP32 S3
const uint8_t macMonitorDebug[]    = {0xA0, 0xB7, 0x65, 0x4B, 0xC5, 0x30};   // ESP32 NodeMCU Dev Kit C V2 mit CP2102

//// Przykładowe współrzędne i rozmiary obszarów ekranu TFT
constexpr int X_POS = 10;
constexpr int Y_POS_FAILED_SEC = 10;
constexpr int Y_POS_TOTAL_FAILED = 30;
constexpr int Y_POS_TOTAL_SENT = 50;
constexpr int RECT_WIDTH = 120;
constexpr int RECT_HEIGHT = 15;



#endif // PARAMETERS_H


