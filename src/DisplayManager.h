#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <TFT_eSPI.h>

class DisplayManager {
private:
    TFT_eSPI tft;
    TFT_eSprite spriteJoystick_L;   // sprite for left joystick display
    TFT_eSprite spriteJoystick_R;   // sprite for right joystick display
    TFT_eSprite spriteMessages;     // sprite for displaying ESP-NOW messages
    TFT_eSprite spriteStatus;       // sprite for displaying status/debug info
    TFT_eSprite spriteButtons_L;    // sprite for left gamepad buttons
    TFT_eSprite spriteButtons_R;    // sprite for right gamepad buttons

    int lastLx, lastLy, lastRx, lastRy;
    int lastPacketsSent, lastErrors;

    // Private helper: draws a filled diamond shape
    void fillDiamond(TFT_eSprite &sprite, int cx, int cy, int size, uint16_t color);

public:
    DisplayManager();
    void begin();
    void updateJoystick(int lx, int ly, int rx, int ry);
    void updateStatus(int lx, int ly, int rx, int ry,
                      bool L_Button_A, bool L_Button_B, bool L_Button_X, bool L_Button_Y,
                      bool L_Button_SELECT, bool L_Button_START,
                      bool R_Button_A, bool R_Button_B, bool R_Button_X, bool R_Button_Y,
                      bool R_Button_SELECT, bool R_Button_START);
    void showMessage(const char* message);
    void updateButtonsL(bool L_Button_A, bool L_Button_B, bool L_Button_X,
                        bool L_Button_Y, bool L_Button_SELECT, bool L_Button_START);
    void updateButtonsR(bool R_Button_A, bool R_Button_B, bool R_Button_X,
                        bool R_Button_Y, bool R_Button_SELECT, bool R_Button_START);
};

#endif // DISPLAY_MANAGER_H
