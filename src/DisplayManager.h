#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <TFT_eSPI.h>

class DisplayManager {
private:
    TFT_eSPI tft;
    TFT_eSprite imgSprite128_160;
    TFT_eSprite imgSprite128_40;
    TFT_eSprite imgSprite64_64;
    TFT_eSprite imgSprite128_44;

    int lastLx, lastLy, lastRx, lastRy;
    int lastPacketsSent, lastErrors;

public:
    DisplayManager();
    void updateJoystick(int lx, int ly, int rx, int ry);
    void updateTerminalInfo(int packetsSent, int errors);
    void drawPointer(int lx, int ly, int rx, int ry);
};

#endif