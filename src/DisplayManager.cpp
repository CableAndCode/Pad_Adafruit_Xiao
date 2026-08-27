#include "DisplayManager.h"
#include <string.h>
#include <math.h>

DisplayManager::DisplayManager()
    : tft(),
      spriteJoystick_L(&tft), spriteJoystick_R(&tft),
      spriteStatus(&tft), spriteLower(&tft),
      lastLx(-1), lastLy(-1), lastRx(-1), lastRy(-1),
      lastElx(-1), lastEly(-1), lastErx(-1), lastEry(-1), lastEchoValid(false),
      lastPanelId(0xFF) {
    memset(lastPanelData, 0, sizeof(lastPanelData));
}

void DisplayManager::begin() {
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    spriteJoystick_L.createSprite(64, 64);
    spriteJoystick_R.createSprite(64, 64);
    spriteStatus.createSprite(128, 24);
    spriteLower.createSprite(LOWER_W, LOWER_H);

    spriteJoystick_L.fillScreen(TFT_BLACK);
    spriteJoystick_R.fillScreen(TFT_BLACK);
    spriteStatus.fillScreen(TFT_BLACK);
    spriteLower.fillScreen(TFT_BLACK);
}

void DisplayManager::clearAll() {
    tft.fillScreen(TFT_BLACK);
}

void DisplayManager::clearLower() {
    tft.fillRect(0, LOWER_Y, LOWER_W, LOWER_H, TFT_BLACK);
}

void DisplayManager::invalidate() {
    lastLx = lastLy = lastRx = lastRy = INT16_MIN;
    lastElx = lastEly = lastErx = lastEry = INT16_MIN;
    lastEchoValid = false;
    lastPanelId = 0xFF;
    memset(lastPanelData, 0, sizeof(lastPanelData));
}

void DisplayManager::showSplash(uint8_t protoVersion, uint32_t buildId,
                                const char* statusLine) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_CYAN);
    tft.setCursor(6, 26);
    tft.print("MECANUM");
    tft.setCursor(6, 46);
    tft.print("PAD");

    tft.drawFastHLine(6, 70, 116, TFT_BLUE);

    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(6, 82);
    tft.printf("protokol v%u", (unsigned)protoVersion);
    tft.setCursor(6, 94);
    tft.printf("build %08X", (unsigned)buildId);

    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(6, 118);
    tft.print(statusLine);

    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(6, 144);
    tft.print("SELECT = ekrany");
}

bool DisplayManager::panelChanged(uint8_t id, const void* data, size_t len) {
    if (len > sizeof(lastPanelData)) len = sizeof(lastPanelData);
    if (id == lastPanelId && memcmp(lastPanelData, data, len) == 0) return false;
    lastPanelId = id;
    memset(lastPanelData, 0, sizeof(lastPanelData));
    memcpy(lastPanelData, data, len);
    return true;
}

void DisplayManager::fillDiamond(TFT_eSprite& sprite, int cx, int cy, int size,
                                 uint16_t color) {
    sprite.fillTriangle(cx, cy - size, cx + size, cy, cx - size, cy, color);
    sprite.fillTriangle(cx, cy + size, cx + size, cy, cx - size, cy, color);
}

// Kropka echa: pozycja osi ODESŁANYCH przez platformę, w tym samym układzie co
// pierścień drążka. W środku = łącze żyje i nadąża, wleczona = opóźnienie,
// skacząca = straty. Brak kropki = telemetria nie dociera.
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
    if (echoValid) spriteJoystick_L.fillCircle(32 + elx / 16, 32 - ely / 16, 1, TFT_WHITE);
    spriteJoystick_L.pushSprite(0, 0);

    spriteJoystick_R.fillScreen(TFT_BLACK);
    spriteJoystick_R.drawRect(0, 0, 64, 64, TFT_WHITE);
    spriteJoystick_R.drawCircle(32 + rx / 16, 32 - ry / 16, 3, TFT_CYAN);
    if (echoValid) spriteJoystick_R.fillCircle(32 + erx / 16, 32 - ery / 16, 1, TFT_WHITE);
    spriteJoystick_R.pushSprite(64, 0);

    lastLx = lx; lastLy = ly; lastRx = rx; lastRy = ry;
    lastElx = elx; lastEly = ely; lastErx = erx; lastEry = ery;
    lastEchoValid = echoValid;
}

// Pas y = 65..88 jest wolny: pierścienie kończą się na 63, panel dolny
// zaczyna na 90.
void DisplayManager::updateLinkStatus(const char* text, uint16_t color) {
    spriteStatus.fillSprite(TFT_BLACK);
    spriteStatus.setCursor(2, 4);
    spriteStatus.setTextColor(color);
    spriteStatus.setTextSize(1);
    spriteStatus.print(text);
    spriteStatus.pushSprite(0, 65);
}

// --- Ekran przycisków: dokładnie to samo rozmieszczenie co wcześniej, tylko
// narysowane w jednym wspólnym sprite'cie zamiast w dwóch osobnych.
void DisplayManager::panelButtons(const bool L[6], const bool R[6]) {
    uint8_t sig[12];
    for (int i = 0; i < 6; i++) { sig[i] = L[i]; sig[i + 6] = R[i]; }
    if (!panelChanged(1, sig, sizeof(sig))) return;

    spriteLower.fillSprite(TFT_BLACK);
    // kolejność w tablicach: A, B, X, Y, SELECT, START
    const int xoff[2] = { 0, 65 };
    const bool* src[2] = { L, R };
    for (int s = 0; s < 2; s++) {
        const bool* b = src[s];
        int x = xoff[s];
        fillDiamond(spriteLower, x + 32, 15, 6, b[3] ? TFT_RED : TFT_WHITE); // Y góra
        fillDiamond(spriteLower, x + 20, 27, 6, b[2] ? TFT_RED : TFT_WHITE); // X lewo
        fillDiamond(spriteLower, x + 44, 27, 6, b[1] ? TFT_RED : TFT_WHITE); // B prawo
        fillDiamond(spriteLower, x + 32, 39, 6, b[0] ? TFT_RED : TFT_WHITE); // A dół
        spriteLower.fillRect(x +  0, 55, 20, 10, b[4] ? TFT_RED : TFT_WHITE); // SELECT
        spriteLower.fillRect(x + 44, 55, 20, 10, b[5] ? TFT_RED : TFT_WHITE); // START
    }
    spriteLower.pushSprite(0, LOWER_Y);
}

void DisplayManager::drawVector(int cx, int cy, int r, float vx, float vy,
                                uint16_t color) {
    // vy = do przodu = w górę ekranu (oś Y ekranu rośnie w dół).
    int ex = cx + (int)lroundf(vx * r);
    int ey = cy - (int)lroundf(vy * r);
    spriteLower.drawLine(cx, cy, ex, ey, color);
    spriteLower.fillCircle(ex, ey, 2, color);
}

// Ekran jazdy: wektor zadany (cyjan) i rzeczywisty (biały), odtworzone
// z obrotów czterech kół przez odwrócenie mieszania mecanum.
void DisplayManager::panelMotion(float tvx, float tvy, float tw,
                                 float mvx, float mvy, float mw, bool valid) {
    int16_t sig[7] = { (int16_t)tvx, (int16_t)tvy, (int16_t)tw,
                       (int16_t)mvx, (int16_t)mvy, (int16_t)mw, (int16_t)valid };
    if (!panelChanged(2, sig, sizeof(sig))) return;

    spriteLower.fillSprite(TFT_BLACK);

    const int cx = 34, cy = 35, r = 30;
    spriteLower.drawCircle(cx, cy, r, TFT_BLUE);
    spriteLower.drawFastHLine(cx - 3, cy, 7, TFT_DARKGREY);
    spriteLower.drawFastVLine(cx, cy - 3, 7, TFT_DARKGREY);

    if (valid) {
        const float maxRpm = (float)MAX_RPM_TELEMETRY;
        drawVector(cx, cy, r, tvx / maxRpm, tvy / maxRpm, TFT_CYAN);
        drawVector(cx, cy, r, mvx / maxRpm, mvy / maxRpm, TFT_WHITE);

        // Prędkość liniowa z długości wektora zmierzonego.
        float rpm  = sqrtf(mvx * mvx + mvy * mvy);
        float mps  = rpm * RPM_TO_MPS;

        spriteLower.setTextSize(1);
        spriteLower.setTextColor(TFT_WHITE);
        spriteLower.setCursor(72, 6);
        spriteLower.printf("%4.2f m/s", mps);
        spriteLower.setTextColor(TFT_CYAN);
        spriteLower.setCursor(72, 20);
        spriteLower.printf("vy %4.0f", mvy);
        spriteLower.setCursor(72, 32);
        spriteLower.printf("vx %4.0f", mvx);
        spriteLower.setCursor(72, 44);
        spriteLower.printf("ob %4.0f", mw);
        spriteLower.setTextColor(TFT_DARKGREY);
        spriteLower.setCursor(72, 58);
        spriteLower.print("0,1 RPM");
    } else {
        spriteLower.setTextSize(1);
        spriteLower.setTextColor(TFT_DARKGREY);
        spriteLower.setCursor(72, 30);
        spriteLower.print("brak");
        spriteLower.setCursor(72, 42);
        spriteLower.print("danych");
    }
    spriteLower.pushSprite(0, LOWER_Y);
}

// Ekran kół: dla każdego koła słupek zmierzonych obrotów i znacznik zadanych.
// Rozjazd znacznika i słupka pokazuje, które koło nie nadąża.
void DisplayManager::panelWheels(const WheelRow rows[4]) {
    if (!panelChanged(3, rows, sizeof(WheelRow) * 4)) return;

    spriteLower.fillSprite(TFT_BLACK);
    static const char* names[4] = { "FL", "FR", "RL", "RR" };

    const int barX = 16, barW = 56, barMid = barX + barW / 2;
    for (int i = 0; i < 4; i++) {
        int y = 2 + i * 17;
        spriteLower.setTextSize(1);
        spriteLower.setTextColor(TFT_DARKGREY);
        spriteLower.setCursor(0, y + 2);
        spriteLower.print(names[i]);

        spriteLower.drawFastVLine(barMid, y, 11, TFT_DARKGREY);

        int m = rows[i].measured * (barW / 2) / (MAX_RPM_TELEMETRY);
        if (m > barW / 2)  m = barW / 2;
        if (m < -barW / 2) m = -barW / 2;
        if (m >= 0) spriteLower.fillRect(barMid, y + 2, m, 7, TFT_CYAN);
        else        spriteLower.fillRect(barMid + m, y + 2, -m, 7, TFT_CYAN);

        int t = rows[i].target * (barW / 2) / (MAX_RPM_TELEMETRY);
        if (t > barW / 2)  t = barW / 2;
        if (t < -barW / 2) t = -barW / 2;
        spriteLower.drawFastVLine(barMid + t, y, 11, TFT_YELLOW);

        spriteLower.setTextColor(TFT_WHITE);
        spriteLower.setCursor(78, y + 2);
        spriteLower.printf("%5.1f", rows[i].measured / 10.0f);
    }
    spriteLower.pushSprite(0, LOWER_Y);
}

void DisplayManager::panelLink(uint32_t rttMs, unsigned lossPermille,
                               uint32_t telemSeq, uint32_t ackErrors,
                               uint32_t protoErrors) {
    uint32_t sig[5] = { rttMs, lossPermille, telemSeq / 25, ackErrors, protoErrors };
    if (!panelChanged(4, sig, sizeof(sig))) return;

    spriteLower.fillSprite(TFT_BLACK);
    spriteLower.setTextSize(1);
    spriteLower.setTextColor(TFT_WHITE);
    spriteLower.setCursor(0, 2);
    spriteLower.printf("RTT     %lu ms", (unsigned long)rttMs);
    spriteLower.setCursor(0, 14);
    spriteLower.setTextColor(lossPermille ? TFT_YELLOW : TFT_WHITE);
    spriteLower.printf("strata  %u/1000", lossPermille);
    spriteLower.setCursor(0, 26);
    spriteLower.setTextColor(TFT_WHITE);
    spriteLower.printf("ramek   %lu", (unsigned long)telemSeq);
    spriteLower.setCursor(0, 38);
    spriteLower.setTextColor(ackErrors ? TFT_YELLOW : TFT_WHITE);
    spriteLower.printf("bez ACK %lu", (unsigned long)ackErrors);
    spriteLower.setCursor(0, 50);
    spriteLower.setTextColor(protoErrors ? TFT_RED : TFT_WHITE);
    spriteLower.printf("bledy   %lu", (unsigned long)protoErrors);
    spriteLower.pushSprite(0, LOWER_Y);
}
