#include<Arduino.h>

typedef struct Message_from_Pad {
    uint16_t seqNum = 0;
    int16_t L_Joystick_x_message = 0;
    int16_t L_Joystick_y_message = 0;
    int16_t R_Joystick_x_message = 0;
    int16_t R_Joystick_y_message = 0;
    uint8_t L_Joystick_buttons_message = 0;
    uint8_t R_Joystick_buttons_message = 0;
    int16_t L_Joystick_raw_x = 0;
    int16_t L_Joystick_raw_y = 0;
    int16_t R_Joystick_raw_x = 0;
    int16_t R_Joystick_raw_y = 0;
}struct_message_from_Pad;

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
} struct_message_from_Platform_Mecanum;


uint8_t macFireBeetle[] = {0xEC, 0x62, 0x60, 0x5A, 0x6E, 0xFC};           //FireBeetle ESP32-E
uint8_t macPlatformMecanum[] = {0xDC, 0xDA, 0x0C, 0x55, 0xD5, 0xB8};      //platforma mecanum z ESP32 S3 DEVKIT C-1 N8R2
uint8_t macModulXiao[] = {0x34, 0x85, 0x18, 0x9E, 0x87, 0xD4};            //Seeduino Xiao ESP32 S3
uint8_t macMonitorDebug[] = {0xA0, 0xB7, 0x65,0x4B, 0xC5, 0x30};          //ESP 32 NodeMCU Dev Kit C V2 mit CP2102








