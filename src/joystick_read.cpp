// joystick_read.cpp
#include "joystick_read.h"
#include <Arduino.h>

JoystickReader::JoystickReader(int offset_x, int offset_y, bool invert_x, bool invert_y) 
    : offsetX(offset_x), offsetY(offset_y), invertX(invert_x), invertY(invert_y), lastValidValueX(0), lastValidValueY(0) {}


void JoystickReader::setOffset(int offset_x, int offset_y) {
    offsetX = offset_x;
    offsetY = offset_y;
}   

int JoystickReader::getCorrectedValueX(int raw_value_x) {
    if (raw_value_x < 0 || raw_value_x > 1023) {
        return lastValidValueX;
    }

    int corrected_value_x;
    
    if (invertX) {
        corrected_value_x = 1024 -raw_value_x - offsetX - 2*(512 - offsetX);
    } else {
        corrected_value_x = raw_value_x - 1024 + offsetX + 2*(512 - offsetX);
    }

    if (abs(corrected_value_x - lastValidValueX) < 3) {
        return lastValidValueX;
    }

    corrected_value_x = constrain(corrected_value_x, -511, 512);
    lastValidValueX = corrected_value_x;
    return corrected_value_x;
}

int JoystickReader::getCorrectedValueY(int raw_value_y) {
    if (raw_value_y < 0 || raw_value_y > 1023) {
        return lastValidValueY;
    }
    int corrected_value_y;
    if (invertY) {
        corrected_value_y = 1024 -raw_value_y - offsetY - 2*(512 - offsetY);
    } else {
        corrected_value_y = raw_value_y - 1024 + offsetY + 2*(512 - offsetY);
    }

    corrected_value_y = constrain(corrected_value_y, -511, 512);
    lastValidValueY = corrected_value_y;
    return corrected_value_y;
}
