#include "DisplayManager.h"

DisplayManager::DisplayManager()
    : tft(), 
      spriteJoystick_L(&tft), spriteJoystick_R(&tft),
      spriteMessages(&tft), spriteStatus(&tft),
      spriteButtons_L(&tft), spriteButtons_R(&tft),
      lastLx(-1), lastLy(-1), lastRx(-1), lastRy(-1),
      lastPacketsSent(-1), lastErrors(-1) {}

// Helper function for drawing a diamond shape
void DisplayManager::fillDiamond(TFT_eSprite &sprite, int cx, int cy, int size, uint16_t color) {
    // Diamond split into two triangles
    sprite.fillTriangle(cx, cy - size, cx + size, cy, cx - size, cy, color);
    sprite.fillTriangle(cx, cy + size, cx + size, cy, cx - size, cy, color);
}

void DisplayManager::begin() {
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    spriteJoystick_L.createSprite(64, 64);
    spriteJoystick_R.createSprite(64, 64);
    spriteMessages.createSprite(128, 30);
    spriteStatus.createSprite(128, 30);
    spriteButtons_L.createSprite(64, 70);
    spriteButtons_R.createSprite(64, 70);

    spriteJoystick_L.fillScreen(TFT_BLACK);
    spriteJoystick_R.fillScreen(TFT_BLACK);
    spriteMessages.fillScreen(TFT_BLACK);
    spriteStatus.fillScreen(TFT_BLACK);
    spriteButtons_L.fillScreen(TFT_BLACK);
    spriteButtons_R.fillScreen(TFT_BLACK);
}

void DisplayManager::updateJoystick(int lx, int ly, int rx, int ry) {
    if (lx != lastLx || ly != lastLy || rx != lastRx || ry != lastRy) {
        spriteJoystick_L.fillScreen(TFT_BLACK);
        spriteJoystick_L.drawRect(0, 0, 64, 64, TFT_WHITE);
        spriteJoystick_L.drawCircle(32 + lx / 16, 32 - ly / 16, 3, TFT_MAGENTA);
        spriteJoystick_L.pushSprite(0, 0);

        spriteJoystick_R.fillScreen(TFT_BLACK);
        spriteJoystick_R.drawRect(0, 0, 64, 64, TFT_WHITE);
        spriteJoystick_R.drawCircle(32 + rx / 16, 32 - ry / 16, 3, TFT_CYAN);
        spriteJoystick_R.pushSprite(64, 0);

        lastLx = lx; lastLy = ly; lastRx = rx; lastRy = ry;
    }
}

void DisplayManager::updateStatus(int lx, int ly, int rx, int ry,
        bool L_Button_A, bool L_Button_B, bool L_Button_X, bool L_Button_Y, bool L_Button_SELECT, bool L_Button_START,
        bool R_Button_A, bool R_Button_B, bool R_Button_X, bool R_Button_Y, bool R_Button_SELECT, bool R_Button_START) {

    spriteStatus.fillScreen(TFT_BLACK);

    spriteStatus.setCursor(0, 3);
    spriteStatus.setTextColor(TFT_WHITE);
    spriteStatus.setTextSize(1);
    spriteStatus.printf("L_X: %4d  R_X: %4d\n", lx, rx);
    spriteStatus.printf("L_Y: %4d  R_Y: %4d\n", ly, ry);


    spriteStatus.pushSprite(0, 65);
}

void DisplayManager::showMessage(const char* message) {
    spriteMessages.setCursor(0, 3);
    spriteMessages.fillScreen(TFT_BLACK);
    spriteMessages.setTextColor(TFT_YELLOW);
    spriteMessages.setTextSize(1);
    spriteMessages.println(message);
    spriteMessages.pushSprite(0, 96);
}

void DisplayManager::updateButtonsL(bool A, bool B, bool X, bool Y, bool Select, bool Start) {
    spriteButtons_L.fillSprite(TFT_BLACK);

    uint16_t colX = X ? TFT_RED : TFT_WHITE;
    uint16_t colY = Y ? TFT_RED : TFT_WHITE;
    uint16_t colB = B ? TFT_RED : TFT_WHITE;
    uint16_t colA = A ? TFT_RED : TFT_WHITE;
    uint16_t colSelect = Select ? TFT_RED : TFT_WHITE;
    uint16_t colStart  = Start  ? TFT_RED : TFT_WHITE;

    fillDiamond(spriteButtons_L, 32, 15, 6, colX);     // Button Y
    fillDiamond(spriteButtons_L, 20, 27, 6, colY);     // Button X
    fillDiamond(spriteButtons_L, 44, 27, 6, colA);     // Button B
    fillDiamond(spriteButtons_L, 32, 39, 6, colB);     // Button A

    spriteButtons_L.fillRect(0, 55, 20, 10, colStart);     // SELECT
    spriteButtons_L.fillRect(44, 55, 20, 10, colSelect);   // START

    spriteButtons_L.pushSprite(0, 90);
}

void DisplayManager::updateButtonsR(bool A, bool B, bool X, bool Y, bool Select, bool Start) {
    spriteButtons_R.fillSprite(TFT_BLACK);

    uint16_t colX = X ? TFT_RED : TFT_WHITE;
    uint16_t colY = Y ? TFT_RED : TFT_WHITE;
    uint16_t colB = B ? TFT_RED : TFT_WHITE;
    uint16_t colA = A ? TFT_RED : TFT_WHITE;
    uint16_t colSelect = Select ? TFT_RED : TFT_WHITE;
    uint16_t colStart  = Start  ? TFT_RED : TFT_WHITE;

    fillDiamond(spriteButtons_R, 32, 15, 6, colB);     // Button Y
    fillDiamond(spriteButtons_R, 20, 27, 6, colA);     // Button X
    fillDiamond(spriteButtons_R, 44, 27, 6, colY);     // Button B
    fillDiamond(spriteButtons_R, 32, 39, 6, colX);     // Button A

    spriteButtons_R.fillRect(0, 55, 20, 10, colSelect);   // SELECT
    spriteButtons_R.fillRect(44, 55, 20, 10, colStart);   // START

    spriteButtons_R.pushSprite(65, 90);
}
