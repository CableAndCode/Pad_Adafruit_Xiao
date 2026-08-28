#include "joystick_read.h"
#include <Arduino.h>

JoystickReader::JoystickReader(int offset_x, int offset_y, bool invert_x, bool invert_y)
    : offsetX(offset_x), offsetY(offset_y),
      lastValidValueX(0), lastValidValueY(0),
      invertX(invert_x), invertY(invert_y) {}

void JoystickReader::setOffset(int offset_x, int offset_y) {
    offsetX = offset_x;
    offsetY = offset_y;
}

int JoystickReader::getCorrectedValueX(int raw_value_x) {
    // A reading outside the ADC range means a bad I2C transfer; hold the last
    // good value rather than letting the sticks jump.
    if (raw_value_x < 0 || raw_value_x > 1023) {
        return lastValidValueX;
    }

    int corrected_value_x;
    if (invertX) {
        corrected_value_x = 1024 - raw_value_x - offsetX - 2 * (512 - offsetX);
    } else {
        corrected_value_x = raw_value_x - 1024 + offsetX + 2 * (512 - offsetX);
    }

    // Hysteresis on the X axis only. NOTE: the Y axis below deliberately has
    // none — this asymmetry is inherited and has never been verified on
    // hardware. Do not "fix" it without a test on the platform: the current
    // steering feel was tuned with it in place.
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
        corrected_value_y = 1024 - raw_value_y - offsetY - 2 * (512 - offsetY);
    } else {
        corrected_value_y = raw_value_y - 1024 + offsetY + 2 * (512 - offsetY);
    }

    corrected_value_y = constrain(corrected_value_y, -511, 512);
    lastValidValueY = corrected_value_y;
    return corrected_value_y;
}
