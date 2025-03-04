// joystick_read.cpp
#include "joystick_read.h"
#include <Arduino.h>

JoystickReader::JoystickReader(int offset_x, int offset_y) 
    : offsetX(offset_x), offsetY(offset_y), lastValidValueX(0), lastValidValueY(0) {}

int JoystickReader::getCorrectedValueX(int raw_value_x) {
    if (raw_value_x < -511 || raw_value_x > 512) {
        return lastValidValueX;
    }

    int corrected_value_x = map(corrected_value_x, -600, 600, -511, 512); // Przeskalowanie na właściwy zakres
    //int corrected_value_x = raw_value_x - offsetX;
    if (abs(corrected_value_x - lastValidValueX) < 3) {
        return lastValidValueX;
    }
    lastValidValueX = corrected_value_x;
    return corrected_value_x;
}

int JoystickReader::getCorrectedValueY(int raw_value_y) {
    if (raw_value_y < -511 || raw_value_y > 512) {
        return lastValidValueY;
    }
    int corrected_value_y = raw_value_y - offsetY;
    if (abs(corrected_value_y - lastValidValueY) < 3) {
        return lastValidValueY;
    }
    //corrected_value_y = map(corrected_value_y, -600, 600, -511, 512); // Przeskalowanie na właściwy zakres
    lastValidValueY = corrected_value_y;
    return corrected_value_y;
}
