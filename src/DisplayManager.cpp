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
    spriteStatus.createSprite(128, 50);
    spriteMessages.createSprite(128, 40);
    
    spriteJoystick_L.fillScreen(TFT_BLACK);
    spriteJoystick_R.fillScreen(TFT_BLACK);
    spriteStatus.fillScreen(TFT_BLACK);
    spriteMessages.fillScreen(TFT_BLACK);
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
        spriteStatus.fillScreen(TFT_BLACK);
        spriteStatus.setCursor(0, 0);
        spriteStatus.setTextColor(TFT_WHITE);
        spriteStatus.setTextSize(1);
        spriteStatus.print("Packets: "); spriteStatus.println(packetsSent);
        spriteStatus.print("Errors: "); spriteStatus.println(errors);
        spriteStatus.print("L_X: "); spriteStatus.println(lastLx);
        spriteStatus.print("L_Y: "); spriteStatus.println(lastLy);
        spriteStatus.print("R_X: "); spriteStatus.println(lastRx);
        spriteStatus.print("R_Y: "); spriteStatus.println(lastRy);
        spriteStatus.pushSprite(0, 64);
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
    spriteMessages.pushSprite(0, 125);
}
