// joystick_read.h
#ifndef JOYSTICK_READ_H
#define JOYSTICK_READ_H

class JoystickReader {
public:
    JoystickReader(int offset_x, int offset_y);
    int getCorrectedValueX(int raw_value_x);
    int getCorrectedValueY(int raw_value_y);

private:
    int offsetX;
    int offsetY;
    int lastValidValueX;
    int lastValidValueY;
};

#endif // JOYSTICK_READ_H