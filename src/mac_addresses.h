#ifndef MAC_ADDRESSES_H
#define MAC_ADDRESSES_H

#include <Arduino.h>

// MAC addresses of devices - adjust to your hardware.
// Copy this file to mac_addresses_private.h and fill in the real addresses;
// the private copy is git-ignored, so your hardware details stay local.

// Mecanum platform - ESP32-S3
uint8_t macPlatformMecanum[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

// Gamepad (pad) - ESP32-S3
uint8_t macPadXiao[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x02};

#endif // MAC_ADDRESSES_H
