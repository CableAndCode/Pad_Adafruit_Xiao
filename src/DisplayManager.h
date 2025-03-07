#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <TFT_eSPI.h>

class DisplayManager {
private:
    TFT_eSPI tft;
    TFT_eSprite spriteJoystick;
    TFT_eSprite spriteStatus;
    TFT_eSprite spriteMessages;

    int lastLx, lastLy, lastRx, lastRy;
    int lastPacketsSent, lastErrors;

public:
    DisplayManager();
    void begin();
    void updateJoystick(int lx, int ly, int rx, int ry);
    void updateStatus(int packetsSent, int errors);
    void showMessage(const char* message);
};

#endif
