// joystick_read.h
#ifndef JOYSTICK_READ_H
#define JOYSTICK_READ_H

class JoystickReader {
public:
    JoystickReader(int offset_x, int offset_y, bool invert_x, bool invert_y);
    int getCorrectedValueX(int raw_value_x);
    int getCorrectedValueY(int raw_value_y);
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