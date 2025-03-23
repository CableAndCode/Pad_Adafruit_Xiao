#include "DisplayManager.h"

DisplayManager::DisplayManager()
    : tft(), 
      spriteJoystick_L(&tft), spriteJoystick_R(&tft),
      spriteMessages(&tft), spriteStatus(&tft),
      spriteButtons_L(&tft), spriteButtons_R(&tft),
      lastLx(-1), lastLy(-1), lastRx(-1), lastRy(-1),
      lastPacketsSent(-1), lastErrors(-1) {}

// Prywatna funkcja pomocnicza do rysowania rombu
void DisplayManager::fillDiamond(TFT_eSprite &sprite, int cx, int cy, int size, uint16_t color) {
    // Romb podzielony na dwa trójkąty
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
    
    // Inicjalizacja sprite’ów przycisków – rozmiar 64x70
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
        spriteJoystick_L.drawCircle(32 + int(lx/16), 32 - int(ly/16), 3, TFT_MAGENTA);
        spriteJoystick_L.pushSprite(0, 0);
        spriteJoystick_R.fillScreen(TFT_BLACK);
        spriteJoystick_R.drawRect(0, 0, 64, 64, TFT_WHITE);
        spriteJoystick_R.drawCircle(32 + int(rx/16), 32 - int(ry/16), 3, TFT_CYAN);
        spriteJoystick_R.pushSprite(64, 0);
        lastLx = lx; lastLy = ly; lastRx = rx; lastRy = ry;
    }
}

void DisplayManager::updateStatus(int lx, int ly, int rx, int ry,
        bool L_Button_A, bool L_Button_B, bool L_Button_X, bool L_Button_Y, bool L_Button_SELECT, bool L_Button_START,
        bool R_Button_A, bool R_Button_B, bool R_Button_X, bool R_Button_Y, bool R_Button_SELECT, bool R_Button_START) {
    
    // Czyścimy obszar statusu
    spriteStatus.fillScreen(TFT_BLACK);
    
    // Wyświetlamy wartości osi joysticków
    spriteStatus.setCursor(0, 3);
    spriteStatus.setTextColor(TFT_WHITE);
    spriteStatus.setTextSize(1);
    spriteStatus.printf("L_X: %4d  R_X: %4d\n", lx, rx);
    spriteStatus.printf("L_Y: %4d  R_Y: %4d\n", ly, ry);
    
    /* Wyświetlamy status przycisków (tekstem) – przykład jak w poprzednim kodzie
    spriteStatus.setCursor(0, 25);
    spriteStatus.setTextSize(1);
    spriteStatus.setTextColor(L_Button_A ? TFT_RED : TFT_WHITE);
    spriteStatus.print("LBA ");
    spriteStatus.setTextColor(L_Button_B ? TFT_RED : TFT_WHITE);
    spriteStatus.print("LBB ");
    spriteStatus.setTextColor(L_Button_X ? TFT_RED : TFT_WHITE);
    spriteStatus.print("LBX ");
    spriteStatus.setTextColor(L_Button_Y ? TFT_RED : TFT_WHITE);
    spriteStatus.print("LBY ");
    spriteStatus.setTextColor(L_Button_SELECT ? TFT_RED : TFT_WHITE);
    spriteStatus.print("LBS ");
    spriteStatus.setTextColor(L_Button_START ? TFT_RED : TFT_WHITE);
    spriteStatus.print("LBSt ");
    
    spriteStatus.setCursor(0, 45);
    spriteStatus.setTextSize(1);
    spriteStatus.setTextColor(R_Button_A ? TFT_RED : TFT_WHITE);
    spriteStatus.print("RBA ");
    spriteStatus.setTextColor(R_Button_B ? TFT_RED : TFT_WHITE);
    spriteStatus.print("RBB ");
    spriteStatus.setTextColor(R_Button_X ? TFT_RED : TFT_WHITE);
    spriteStatus.print("RBX ");
    spriteStatus.setTextColor(R_Button_Y ? TFT_RED : TFT_WHITE);
    spriteStatus.print("RBY ");
    spriteStatus.setTextColor(R_Button_SELECT ? TFT_RED : TFT_WHITE);
    spriteStatus.print("RBS ");
    spriteStatus.setTextColor(R_Button_START ? TFT_RED : TFT_WHITE);
    spriteStatus.print("RBSt ");
    */
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

void DisplayManager::updateButtonsL(bool L_Button_A, bool L_Button_B, bool L_Button_X, bool L_Button_Y, bool L_Button_SELECT, bool L_Button_START) {
    // Czyścimy sprite przycisków lewego gamepada
    spriteButtons_L.fillSprite(TFT_BLACK);
    
    // Ustalamy kolor dla poszczególnych przycisków
    uint16_t colX      = L_Button_X      ? TFT_RED : TFT_WHITE;
    uint16_t colY      = L_Button_Y      ? TFT_RED : TFT_WHITE;
    uint16_t colB      = L_Button_B      ? TFT_RED : TFT_WHITE;
    uint16_t colA      = L_Button_A      ? TFT_RED : TFT_WHITE;
    uint16_t colSelect = L_Button_SELECT ? TFT_RED : TFT_WHITE;
    uint16_t colStart  = L_Button_START  ? TFT_RED : TFT_WHITE;
    
    // Rysujemy przyciski w formie diamentu (współrzędne dobrane do obszaru 64x70)
    fillDiamond(spriteButtons_L, 32, 15, 6, colX); // przycisk Y
    fillDiamond(spriteButtons_L, 20, 27, 6, colY); // przycisk X
    fillDiamond(spriteButtons_L, 44, 27, 6, colA); // przycisk B
    fillDiamond(spriteButtons_L, 32, 39, 6, colB); // przycisk A
    
    // Prostokąty dla przycisków SELECT i START po bokach
    spriteButtons_L.fillRect(0, 55, 20, 10, colStart);   // SELECT po lewej stronie
    spriteButtons_L.fillRect(44, 55, 20, 10, colSelect);     // START po prawej stronie
    
    // Wypchniemy sprite na ekran – przykładowa pozycja (możesz dostosować)
    spriteButtons_L.pushSprite(0, 90);
}

void DisplayManager::updateButtonsR(bool R_Button_A, bool R_Button_B, bool R_Button_X, bool R_Button_Y, bool R_Button_SELECT, bool R_Button_START) {
    spriteButtons_R.fillSprite(TFT_BLACK);
    
    uint16_t colX      = R_Button_X      ? TFT_RED : TFT_WHITE;
    uint16_t colY      = R_Button_Y      ? TFT_RED : TFT_WHITE;
    uint16_t colB      = R_Button_B      ? TFT_RED : TFT_WHITE;
    uint16_t colA      = R_Button_A      ? TFT_RED : TFT_WHITE;
    uint16_t colSelect = R_Button_SELECT ? TFT_RED : TFT_WHITE;
    uint16_t colStart  = R_Button_START  ? TFT_RED : TFT_WHITE;
    
    fillDiamond(spriteButtons_R, 32, 15, 6, colB); // przycisk Y
    fillDiamond(spriteButtons_R, 20, 27, 6, colA); // przycisk X
    fillDiamond(spriteButtons_R, 44, 27, 6, colY); // przycisk B
    fillDiamond(spriteButtons_R, 32, 39, 6, colX); // przycisk A
    
    // Prostokąty dla przycisków SELECT i START po bokach
    spriteButtons_R.fillRect(0, 55, 20, 10, colSelect);   // SELECT po lewej stronie
    spriteButtons_R.fillRect(44, 55, 20, 10, colStart);     // START po prawej stronie
    
    // Przykładowa pozycja wyświetlania sprite’u dla prawego gamepada
    spriteButtons_R.pushSprite(65, 90);
}

