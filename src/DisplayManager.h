#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <TFT_eSPI.h>

class DisplayManager {
private:
    TFT_eSPI tft;
    TFT_eSprite spriteJoystick_L;   // sprite do wyświetlania joysticka lewego
    TFT_eSprite spriteJoystick_R;   // sprite do wyświetlania joysticka prawego   
    TFT_eSprite spriteMessages;     // sprite do wyświetlania komunikatów (ESP-NOW)
    TFT_eSprite spriteStatus;       // sprite do wyświetlania statystyk
    TFT_eSprite spriteButtons_L;    // sprite do wyświetlania przycisków lewego gamepada
    TFT_eSprite spriteButtons_R;    // sprite do wyświetlania przycisków prawego gamepada

    int lastLx, lastLy, lastRx, lastRy;
    int lastPacketsSent, lastErrors;

    // Prywatna funkcja pomocnicza rysująca wypełniony romb
    void fillDiamond(TFT_eSprite &sprite, int cx, int cy, int size, uint16_t color);

public:
    DisplayManager();
    void begin();
    void updateJoystick(int lx, int ly, int rx, int ry);
    void updateStatus(int lx, int ly, int rx, int ry, 
            bool L_Button_A, bool L_Button_B, bool L_Button_X, bool L_Button_Y, bool L_Button_SELECT, bool L_Button_START, 
            bool R_Button_A, bool R_Button_B, bool R_Button_X, bool R_Button_Y, bool R_Button_SELECT, bool R_Button_START);
    void showMessage(const char* message);
    
    // Nowe metody do rysowania przycisków (d-pad) dla lewego i prawego gamepada
    void updateButtonsL(bool L_Button_A, bool L_Button_B, bool L_Button_X, bool L_Button_Y, bool L_Button_SELECT, bool L_Button_START);
    void updateButtonsR(bool R_Button_A, bool R_Button_B, bool R_Button_X, bool R_Button_Y, bool R_Button_SELECT, bool R_Button_START);
};

#endif
