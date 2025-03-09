#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <TFT_eSPI.h>

class DisplayManager {
private:
    TFT_eSPI tft;
    TFT_eSprite spriteJoystick_L;   // sprite do wyświetlania joysticka lewego jako kółka w kwadracie
    TFT_eSprite spriteJoystick_R;   // sprite do wyświetlania joysticka prawego jako kółka w kwadracie   
    TFT_eSprite spriteMessages;     // sprite do wyświetlania komunikatów na temat esp-now
    TFT_eSprite spriteStatus;       // sprite do wyświetlania statusu, czyli ilości wysłanych pakietów i błędów oraz wartości joysticków
    

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
