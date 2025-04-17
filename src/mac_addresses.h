#ifndef MAC_ADDRESSES_H
#define MAC_ADDRESSES_H

#include <Arduino.h>

// MAC addresses of devices – adjust to your hardware

// Mecanum platform – ESP32-S3
uint8_t macPlatformMecanum[] = {0xDC, 0xDA, 0x0C, 0x55, 0xD5, 0xB8}; 

// Gamepad (Pad 1) – ESP32-S3
uint8_t macModulXiao[] = {0x34, 0x85, 0x18, 0x9E, 0x87, 0xD4};  

// Debug monitor – ESP32-LX6
uint8_t macMonitorDebug[] = {0xA0, 0xB7, 0x65, 0x4B, 0xC5, 0x30};

#endif // MAC_ADDRESSES_H
