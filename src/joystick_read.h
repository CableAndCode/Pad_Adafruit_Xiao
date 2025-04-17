// joystick_read.h
#ifndef JOYSTICK_READ_H
#define JOYSTICK_READ_H

class JoystickReader {
public:
    // Constructor with offset and axis inversion options
    JoystickReader(int offset_x, int offset_y, bool invert_x, bool invert_y);

    // Returns normalized X value based on raw input
    int getCorrectedValueX(int raw_value_x);

    // Returns normalized Y value based on raw input
    int getCorrectedValueY(int raw_value_y);

    // Set new offset values for X and Y axes
    void setOffset(int offset_x, int offset_y);

private:
    int offsetX;
    int offsetY;
    int lastValidValueX;
    int lastValidValueY;
    int corrected_value_x;
    int corrected_value_y;
    bool invertX;
    bool invertY;
};

#endif // JOYSTICK_READ_H
