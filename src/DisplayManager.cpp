#include "DisplayManager.h"

DisplayManager::DisplayManager()
    : tft(), imgSprite128_160(&tft), imgSprite128_40(&tft), imgSprite64_64(&tft), imgSprite128_44(&tft),
      lastLx(-1), lastLy(-1), lastRx(-1), lastRy(-1), lastPacketsSent(-1), lastErrors(-1) {
    
    tft.init();
    tft.setRotation(0);
    tft.setTextColor(TFT_WHITE);
    imgSprite128_160.createSprite(128, 160);
    imgSprite128_40.createSprite(128, 40);
    imgSprite64_64.createSprite(64, 64);
    imgSprite128_44.createSprite(128, 44);
}


void DisplayManager::updateJoystick(int lx, int ly, int rx, int ry) {
    if (lx != lastLx || ly != lastLy || rx != lastRx || ry != lastRy) {
        imgSprite128_40.fillScreen(TFT_BLACK);
        imgSprite128_40.drawString("LX:", 10, 1, 2);
        imgSprite128_40.drawString(String(lx), 35, 1, 2);
        imgSprite128_40.drawString("LY:", 10, 14, 2);
        imgSprite128_40.drawString(String(ly), 35, 14, 2);
        imgSprite128_40.drawString("RX:", 74, 1, 2);
        imgSprite128_40.drawString(String(rx), 100, 1, 2);
        imgSprite128_40.drawString("RY:", 74, 14, 2);
        imgSprite128_40.drawString(String(ry), 100, 14, 2);
        imgSprite128_40.pushSprite(0,65);

        lastLx = lx;
        lastLy = ly;
        lastRx = rx;
        lastRy = ry;
    }
}

void DisplayManager::updateTerminalInfo(int packetsSent, int errors) {
    if (packetsSent != lastPacketsSent || errors != lastErrors) {
        imgSprite128_44.fillScreen(TFT_BLACK);
        imgSprite128_44.drawString("Sent:", 1, 1);
        imgSprite128_44.drawString(String(packetsSent), 110, 1);
        imgSprite128_44.drawString("Errors:", 1, 10);
        imgSprite128_44.drawString(String(errors), 110, 10);
        imgSprite128_44.pushSprite(0,105);

        lastPacketsSent = packetsSent;
        lastErrors = errors;
    }
}

void DisplayManager::drawPointer(int lx, int ly, int rx, int ry) {
    imgSprite64_64.fillScreen(TFT_BLACK);
    imgSprite64_64.fillCircle(64 - lx, ly, 2, TFT_MAGENTA);
    imgSprite64_64.fillCircle(64 - rx, ry, 2, TFT_CYAN);
    imgSprite64_64.drawRect(0, 0, 64, 64, TFT_WHITE);
    imgSprite64_64.pushSprite(0, 0);
}
