#ifndef JOYSTICK_READ_H
#define JOYSTICK_READ_H

/**
 * Turns a raw 10-bit seesaw ADC reading into a calibrated, signed value
 * centred on zero, in the range roughly -511..+512.
 *
 * The offset is the reading taken at boot with the stick at rest, so each
 * device calibrates itself rather than relying on a nominal centre.
 */
class JoystickReader {
public:
    JoystickReader(int offset_x, int offset_y, bool invert_x, bool invert_y);

    /// Calibrated X value. Out-of-range readings return the last valid one.
    int getCorrectedValueX(int raw_value_x);

    /// Calibrated Y value. Out-of-range readings return the last valid one.
    int getCorrectedValueY(int raw_value_y);

    /// Replaces the resting-position offsets (called once, after boot).
    void setOffset(int offset_x, int offset_y);

private:
    int  offsetX;
    int  offsetY;
    int  lastValidValueX;
    int  lastValidValueY;
    bool invertX;
    bool invertY;
};

#endif // JOYSTICK_READ_H
