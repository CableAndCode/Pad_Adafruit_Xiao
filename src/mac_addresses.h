#ifndef MAC_ADDRESSES_H
#define MAC_ADDRESSES_H

#include <Arduino.h>

// MAC addresses of devices – adjust to your hardware

// Mecanum platform – ESP32-S3
uint8_t macPlatformMecanum[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01}; 

// Gamepad (Pad 1) – ESP32-S3
uint8_t macModulXiao[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};  

// Debug monitor – ESP32-LX6
uint8_t macMonitorDebug[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

#endif // MAC_ADDRESSES_H
