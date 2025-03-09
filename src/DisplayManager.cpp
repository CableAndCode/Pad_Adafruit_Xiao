#include "DisplayManager.h"

DisplayManager::DisplayManager()
    : tft(), spriteJoystick_L(&tft),spriteJoystick_R(&tft) , spriteStatus(&tft), spriteMessages(&tft),
      lastLx(-1), lastLy(-1), lastRx(-1), lastRy(-1),
      lastPacketsSent(-1), lastErrors(-1) {}

void DisplayManager::begin() {
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    
    spriteJoystick_L.createSprite(64, 64);
    spriteJoystick_R.createSprite(64, 64);
    spriteMessages.createSprite(128, 30);
    spriteStatus.createSprite(128, 40);
    
    
    spriteJoystick_L.fillScreen(TFT_BLACK);
    spriteJoystick_R.fillScreen(TFT_BLACK);
    spriteMessages.fillScreen(TFT_BLACK);
    spriteStatus.fillScreen(TFT_BLACK);
}

void DisplayManager::updateJoystick(int lx, int ly, int rx, int ry) {
    if (lx != lastLx || ly != lastLy || rx != lastRx || ry != lastRy) {
        spriteJoystick_L.fillScreen(TFT_BLACK);
        spriteJoystick_L.drawRect(0, 0, 64, 64, TFT_WHITE);
        spriteJoystick_L.drawCircle(32 + int(lx/16), 32 - int(ly/16), 3, TFT_MAGENTA);
        spriteJoystick_L.pushSprite(0, 0);
        spriteJoystick_R.fillScreen(TFT_BLACK);
        spriteJoystick_R.drawRect(0, 0, 64, 64, TFT_WHITE);
        spriteJoystick_R.drawCircle(32 + int(rx/16), 32 - int(ry/16), 3, TFT_CYAN);
        spriteJoystick_R.pushSprite(64, 0);
        lastLx = lx; lastLy = ly; lastRx = rx; lastRy = ry;
    }
}

void DisplayManager::updateStatus(int packetsSent, int errors) {
    if (packetsSent != lastPacketsSent || errors != lastErrors) {
        // Zmiana koloru tła na podstawie liczby błędów
        if (errors == 0) {
            spriteStatus.fillScreen(TFT_BLACK);
        } else if (errors < 50) {
            spriteStatus.fillScreen(TFT_CYAN);
        } else {
            spriteStatus.fillScreen(TFT_PURPLE);
        }
        spriteStatus.setCursor(0, 3);
        spriteStatus.setTextColor(TFT_WHITE);
        spriteStatus.setTextSize(1);

        spriteStatus.printf("P: %6d  E: %6d\n", packetsSent, errors);
        spriteStatus.printf("L_X: %4d  L_Y: %4d\n", lastLx, lastLy);
        spriteStatus.printf("R_X: %4d  R_Y: %4d\n", lastRx, lastRy);
        spriteStatus.drawFastHLine(0, 30, 128, TFT_WHITE);

        spriteStatus.pushSprite(0, 65);

    }
}

void DisplayManager::showMessage(const char* message) {
    spriteMessages.setCursor(0, 3);
    spriteMessages.fillScreen(TFT_BLACK);
    spriteMessages.setTextColor(TFT_YELLOW);
    spriteMessages.setTextSize(1);
    spriteMessages.println(message);
    spriteMessages.pushSprite(0, 96);
}
