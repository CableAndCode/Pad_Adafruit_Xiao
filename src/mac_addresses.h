#ifndef MAC_ADDRESSES_H
#define MAC_ADDRESSES_H

#include <Arduino.h>


// MAC adresy urządzeń – dostosuj do swojego sprzętu
//płytka nie wykorzystywana, z baterią LX6
uint8_t macFireBeetle[]         = {0xEC, 0x62, 0x60, 0x5A, 0x6E, 0xFC}; 

//Platforma Mecanum – ESP32-S3
uint8_t macPlatformMecanum[]    = {0xDC, 0xDA, 0x0C, 0x55, 0xD5, 0xB8}; 

//Pad 1 – ESP32-S3
uint8_t macModulXiao[]          = {0x34, 0x85, 0x18, 0x9E, 0x87, 0xD4};  

//Monitor debug – ESP32-LX6
uint8_t macMonitorDebug[]       = {0xA0, 0xB7, 0x65, 0x4B, 0xC5, 0x30};          



#endif // MAC_ADDRESSES_H
