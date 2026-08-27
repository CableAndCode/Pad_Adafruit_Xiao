#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <TFT_eSPI.h>
#include "messages.h"

// Podział ekranu 128x160 — stały dla wszystkich widoków:
//   y   0..63   pierścienie drążków z kropkami echa (nie zmieniają się)
//   y  65..88   pas stanu łącza
//   y  90..159  panel dolny — treść zależna od wybranego ekranu
//
// Panel dolny to JEDEN sprite współdzielony przez wszystkie widoki. Dwa dawne
// sprite'y przycisków (64x70 każdy) zajmowały dokładnie tyle samo pamięci co
// jeden 128x70, więc samo złożenie ich niczego nie zwolniło — sens jest inny:
// każdy kolejny ekran rysuje się w tym samym buforze zamiast alokować własny.
// Trzy nowe panele w osobnych sprite'ach kosztowałyby ponad 50 kB.
constexpr int LOWER_Y = 90;
constexpr int LOWER_W = 128;
constexpr int LOWER_H = 70;

struct WheelRow {
    int16_t target;    // 0,1 RPM, konwencja robota
    int16_t measured;
    int16_t pwm;
};

class DisplayManager {
public:
    DisplayManager();
    void begin();

    /// Ekran powitalny — rysowany prosto na ekran, bez sprite'ów.
    void showSplash(uint8_t protoVersion, uint32_t buildId, const char* statusLine);

    void clearAll();     ///< czyści cały ekran (wyjście ze splasha)
    void clearLower();   ///< czyści sam panel dolny (zmiana ekranu)

    /// Wymusza przerysowanie wszystkiego (po zejściu ze splasha lub zmianie ekranu).
    void invalidate();

    void updateJoystick(int lx, int ly, int rx, int ry,
                        bool echoValid, int elx, int ely, int erx, int ery);
    void updateLinkStatus(const char* text, uint16_t color);

    // --- panele dolne, każdy rysuje tylko przy zmianie danych ---
    void panelButtons(const bool L[6], const bool R[6]);
    void panelMotion(float tvx, float tvy, float tw,   // zadane, 0,1 RPM
                     float mvx, float mvy, float mw,   // zmierzone, 0,1 RPM
                     bool valid);
    void panelWheels(const WheelRow rows[4]);
    void panelLink(uint32_t rttMs, unsigned lossPermille,
                   uint32_t telemSeq, uint32_t ackErrors, uint32_t protoErrors);

private:
    TFT_eSPI tft;
    TFT_eSprite spriteJoystick_L;
    TFT_eSprite spriteJoystick_R;
    TFT_eSprite spriteStatus;
    TFT_eSprite spriteLower;

    int  lastLx, lastLy, lastRx, lastRy;
    int  lastElx, lastEly, lastErx, lastEry;
    bool lastEchoValid;

    // Sygnatury paneli — porównywane bajt w bajt, żeby nie przerysowywać
    // dolnej połowy ekranu bez powodu.
    uint8_t lastPanelId;
    uint8_t lastPanelData[32];

    bool panelChanged(uint8_t id, const void* data, size_t len);
    void fillDiamond(TFT_eSprite& sprite, int cx, int cy, int size, uint16_t color);
    void drawVector(int cx, int cy, int r, float vx, float vy, uint16_t color);
};

#endif // DISPLAY_MANAGER_H
