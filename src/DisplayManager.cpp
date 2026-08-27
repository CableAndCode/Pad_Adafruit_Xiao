#include "DisplayManager.h"

DisplayManager::DisplayManager()
    : tft(), 
      spriteJoystick_L(&tft), spriteJoystick_R(&tft),
      spriteMessages(&tft), spriteStatus(&tft),
      spriteButtons_L(&tft), spriteButtons_R(&tft),
      lastLx(-1), lastLy(-1), lastRx(-1), lastRy(-1),
      lastElx(-1), lastEly(-1), lastErx(-1), lastEry(-1), lastEchoValid(false),
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
    spriteStatus.createSprite(128, 24);
    spriteButtons_L.createSprite(64, 70);
    spriteButtons_R.createSprite(64, 70);

    spriteJoystick_L.fillScreen(TFT_BLACK);
    spriteJoystick_R.fillScreen(TFT_BLACK);
    spriteMessages.fillScreen(TFT_BLACK);
    spriteStatus.fillScreen(TFT_BLACK);
    spriteButtons_L.fillScreen(TFT_BLACK);
    spriteButtons_R.fillScreen(TFT_BLACK);
}

// Kropka echa: pozycja osi ODESŁANYCH przez platformę, narysowana w tym samym
// układzie co pierścień drążka. Łącze żywe i nadążające = kropka siedzi
// w środku pierścienia. Opóźnienie = wlecze się za nim. Straty = skacze.
// To jedyny sygnał mówiący, że platforma czyta WŁAŚCIWE pola — zgodna wersja
// protokołu dowodzi tylko, że obie strony mają ten sam plik.
void DisplayManager::updateJoystick(int lx, int ly, int rx, int ry,
                                    bool echoValid, int elx, int ely, int erx, int ery) {
    if (lx == lastLx && ly == lastLy && rx == lastRx && ry == lastRy &&
        echoValid == lastEchoValid &&
        (!echoValid || (elx == lastElx && ely == lastEly &&
                        erx == lastErx && ery == lastEry))) {
        return;
    }

    spriteJoystick_L.fillScreen(TFT_BLACK);
    spriteJoystick_L.drawRect(0, 0, 64, 64, TFT_WHITE);
    spriteJoystick_L.drawCircle(32 + lx / 16, 32 - ly / 16, 3, TFT_MAGENTA);
    if (echoValid) {
        spriteJoystick_L.fillCircle(32 + elx / 16, 32 - ely / 16, 1, TFT_WHITE);
    }
    spriteJoystick_L.pushSprite(0, 0);

    spriteJoystick_R.fillScreen(TFT_BLACK);
    spriteJoystick_R.drawRect(0, 0, 64, 64, TFT_WHITE);
    spriteJoystick_R.drawCircle(32 + rx / 16, 32 - ry / 16, 3, TFT_CYAN);
    if (echoValid) {
        spriteJoystick_R.fillCircle(32 + erx / 16, 32 - ery / 16, 1, TFT_WHITE);
    }
    spriteJoystick_R.pushSprite(64, 0);

    lastLx = lx; lastLy = ly; lastRx = rx; lastRy = ry;
    lastElx = elx; lastEly = ely; lastErx = erx; lastEry = ery;
    lastEchoValid = echoValid;
}

// Pas y = 65..88 jest wolny: ramki joysticków kończą się na 63, sprite'y
// przycisków zaczynają na 90. Nic tu wcześniej nie było rysowane — poprzednia
// wersja updateStatus() nie była wołana z żadnego miejsca w programie.
void DisplayManager::updateLinkStatus(const char* text, uint16_t color) {
    spriteStatus.fillSprite(TFT_BLACK);
    spriteStatus.setCursor(2, 4);
    spriteStatus.setTextColor(color);
    spriteStatus.setTextSize(1);
    spriteStatus.print(text);
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
