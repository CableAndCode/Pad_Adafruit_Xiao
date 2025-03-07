#include "DisplayManager.h"

DisplayManager::DisplayManager()
    : tft(), spriteJoystick(&tft), spriteStatus(&tft), spriteMessages(&tft),
      lastLx(-1), lastLy(-1), lastRx(-1), lastRy(-1),
      lastPacketsSent(-1), lastErrors(-1) {}

void DisplayManager::begin() {
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    
    spriteJoystick.createSprite(128, 64);
    spriteStatus.createSprite(128, 40);
    spriteMessages.createSprite(128, 40);
    
    spriteJoystick.fillScreen(TFT_BLACK);
    spriteStatus.fillScreen(TFT_BLACK);
    spriteMessages.fillScreen(TFT_BLACK);
}

void DisplayManager::updateJoystick(int lx, int ly, int rx, int ry) {
    if (lx != lastLx || ly != lastLy || rx != lastRx || ry != lastRy) {
        spriteJoystick.fillScreen(TFT_BLACK);
        spriteJoystick.drawCircle(64 - lx, ly, 3, TFT_MAGENTA);
        spriteJoystick.drawCircle(64 - rx, ry, 3, TFT_CYAN);
        spriteJoystick.pushSprite(0, 0);
        lastLx = lx; lastLy = ly; lastRx = rx; lastRy = ry;
    }
}

void DisplayManager::updateStatus(int packetsSent, int errors) {
    if (packetsSent != lastPacketsSent || errors != lastErrors) {
        spriteStatus.fillScreen(TFT_BLACK);
        spriteStatus.setCursor(0, 0);
        spriteStatus.setTextColor(TFT_WHITE);
        spriteStatus.setTextSize(1);
        spriteStatus.print("Packets: "); spriteStatus.println(packetsSent);
        spriteStatus.print("Errors: "); spriteStatus.println(errors);
        spriteStatus.pushSprite(0, 65);
        lastPacketsSent = packetsSent;
        lastErrors = errors;
    }
}

void DisplayManager::showMessage(const char* message) {
    spriteMessages.fillScreen(TFT_BLACK);
    spriteMessages.setCursor(0, 0);
    spriteMessages.setTextColor(TFT_YELLOW);
    spriteMessages.setTextSize(1);
    spriteMessages.println(message);
    spriteMessages.pushSprite(0, 105);
}
