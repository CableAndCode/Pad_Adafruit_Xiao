#include "DisplayManager.h"
#include <string.h>
#include <math.h>

DisplayManager::DisplayManager()
    : tft(),
      spriteJoystick_L(&tft), spriteJoystick_R(&tft),
      spriteStatus(&tft), spriteLower(&tft), spriteRadar(&tft),
      lastLx(-1), lastLy(-1), lastRx(-1), lastRy(-1),
      lastElx(-1), lastEly(-1), lastErx(-1), lastEry(-1), lastEchoValid(false),
      lastPanelId(0xFF) {
    memset(lastPanelData, 0, sizeof(lastPanelData));
}

void DisplayManager::begin() {
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    // createSprite returns nullptr when it runs out of memory and reports NO
    // error — the sprite simply stops drawing anything. Without this check that
    // would be a nasty fault to track down: an empty rectangle and no trace in
    // any log.
    void* buffers[5] = {
        spriteJoystick_L.createSprite(64, 64),
        spriteJoystick_R.createSprite(64, 64),
        spriteStatus.createSprite(128, 24),
        spriteLower.createSprite(LOWER_W, LOWER_H),
        spriteRadar.createSprite(RADAR_W, RADAR_H)
    };
    for (int i = 0; i < 5; i++) {
        if (buffers[i] == nullptr) {
            Serial.printf("Out of memory for sprite %d!\n", i);
        }
    }

    spriteJoystick_L.fillScreen(TFT_BLACK);
    spriteJoystick_R.fillScreen(TFT_BLACK);
    spriteStatus.fillScreen(TFT_BLACK);
    spriteLower.fillScreen(TFT_BLACK);
    spriteRadar.fillScreen(TFT_BLACK);
}

void DisplayManager::clearAll() {
    tft.fillScreen(TFT_BLACK);
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
    tft.printf("protocol v%u", (unsigned)protoVersion);
    tft.setCursor(6, 94);
    tft.printf("build %08X", (unsigned)buildId);

    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(6, 118);
    tft.print(statusLine);

    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(6, 144);
    tft.print("SELECT = screens");
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

// The echo dot: the position of the axes SENT BACK by the platform, drawn in
// the same frame as the stick ring. Centred = the link is alive and keeping up,
// trailing = latency, jumping = losses. No dot = no telemetry arriving.
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

// The band at y = 65..88 is free: the rings end at 63 and the lower panel
// starts at 90. On the radar layout the same sprite is pushed to y = 0.
void DisplayManager::updateLinkStatus(const char* text, uint16_t color, int y) {
    spriteStatus.fillSprite(TFT_BLACK);
    spriteStatus.setCursor(2, 4);
    spriteStatus.setTextColor(color);
    spriteStatus.setTextSize(1);
    spriteStatus.print(text);
    spriteStatus.pushSprite(0, y);
}

// --- Button screen: purely a check that every button reports. Not needed while
// driving, which is why it lives on a screen of its own.
void DisplayManager::panelButtons(const bool L[6], const bool R[6]) {
    uint8_t sig[12];
    for (int i = 0; i < 6; i++) { sig[i] = L[i]; sig[i + 6] = R[i]; }
    if (!panelChanged(1, sig, sizeof(sig))) return;

    spriteLower.fillSprite(TFT_BLACK);
    // array order: A, B, X, Y, SELECT, START
    const int xoff[2] = { 0, 65 };
    const bool* src[2] = { L, R };
    for (int s = 0; s < 2; s++) {
        const bool* b = src[s];
        int x = xoff[s];
        fillDiamond(spriteLower, x + 32, 15, 6, b[3] ? TFT_RED : TFT_WHITE); // Y, top
        fillDiamond(spriteLower, x + 20, 27, 6, b[2] ? TFT_RED : TFT_WHITE); // X, left
        fillDiamond(spriteLower, x + 44, 27, 6, b[1] ? TFT_RED : TFT_WHITE); // B, right
        fillDiamond(spriteLower, x + 32, 39, 6, b[0] ? TFT_RED : TFT_WHITE); // A, bottom
        spriteLower.fillRect(x +  0, 55, 20, 10, b[4] ? TFT_RED : TFT_WHITE); // SELECT
        spriteLower.fillRect(x + 44, 55, 20, 10, b[5] ? TFT_RED : TFT_WHITE); // START
    }
    spriteLower.pushSprite(0, LOWER_Y);
}

void DisplayManager::drawVector(TFT_eSprite& sp, int cx, int cy, int r,
                                float vx, float vy, uint16_t color) {
    // vy = forward = up the screen (the screen Y axis grows downwards).
    int ex = cx + (int)lroundf(vx * r);
    int ey = cy - (int)lroundf(vy * r);
    sp.drawLine(cx, cy, ex, ey, color);
    sp.fillCircle(ex, ey, 2, color);
}

// Rotation drawn as an arc along the rim: starting at twelve o'clock, length
// proportional to the rotation rate, direction following the sign. The arc is
// relative by nature — how close to the maximum, and which way — which is what
// you read at a glance while driving; the figure in degrees per second is
// printed separately. Drawn pixel by pixel rather than relying on drawArc,
// which is not present in every version of TFT_eSPI.
void DisplayManager::drawSpinArc(TFT_eSprite& sp, int cx, int cy, int r,
                                 float frac, uint16_t color) {
    if (frac > 1.0f)  frac = 1.0f;
    if (frac < -1.0f) frac = -1.0f;
    const float sweep = frac * 300.0f;                 // at most 300 degrees
    if (fabsf(sweep) < 2.0f) return;

    const int steps = (int)fabsf(sweep);
    for (int i = 0; i <= steps; i++) {
        float a = (sweep >= 0 ? i : -i) - 90.0f;        // 0 degrees = twelve o'clock
        float rad = a * 3.14159265f / 180.0f;
        int x = cx + (int)lroundf(cosf(rad) * r);
        int y = cy + (int)lroundf(sinf(rad) * r);
        sp.drawPixel(x, y, color);
        sp.drawPixel(x + 1, y, color);
    }
}

// Radar: full screen, the commanded vector (cyan) and the actual one (white),
// with rotation arcs in the same colours along the rim. The edge of the circle
// is MAX_RPM, so a white vector that never reaches it at full stick deflection
// means MAX_RPM is set too high. That is how the encoder scaling error was
// caught: the white vector sat AT the rim while a stopwatch said the platform
// was doing half that speed.
void DisplayManager::panelRadar(float tvx, float tvy, float tw,
                                float mvx, float mvy, float mw, bool valid) {
    int16_t sig[7] = { (int16_t)tvx, (int16_t)tvy, (int16_t)tw,
                       (int16_t)mvx, (int16_t)mvy, (int16_t)mw, (int16_t)valid };
    if (!panelChanged(5, sig, sizeof(sig))) return;

    spriteRadar.fillSprite(TFT_BLACK);
    const int cx = 64, cy = 62, r = 54;

    spriteRadar.drawCircle(cx, cy, r, TFT_BLUE);          // full speed
    spriteRadar.drawCircle(cx, cy, r / 2, 0x18E3);        // half speed, dimmed
    spriteRadar.drawFastHLine(cx - 4, cy, 9, TFT_DARKGREY);
    spriteRadar.drawFastVLine(cx, cy - 4, 9, TFT_DARKGREY);

    if (valid) {
        const float maxRpm = (float)MAX_RPM_TELEMETRY;
        drawSpinArc(spriteRadar, cx, cy, r - 3,  tw / maxRpm, TFT_CYAN);
        drawSpinArc(spriteRadar, cx, cy, r - 9,  mw / maxRpm, TFT_WHITE);
        drawVector(spriteRadar, cx, cy, r, tvx / maxRpm, tvy / maxRpm, TFT_CYAN);
        drawVector(spriteRadar, cx, cy, r, mvx / maxRpm, mvy / maxRpm, TFT_WHITE);

        float rpm = sqrtf(mvx * mvx + mvy * mvy);
        spriteRadar.setTextSize(2);
        spriteRadar.setTextColor(TFT_WHITE);
        spriteRadar.setCursor(18, 120);
        spriteRadar.printf("%4.2f m/s", rpm * RPM_TO_MPS);

        // Rotation in degrees per second — honest now that the track width is
        // measured. The arc stays alongside it: the number says how much, the
        // arc says which way and how close to the maximum, and that is readable
        // out of the corner of an eye.
        spriteRadar.setTextSize(1);
        spriteRadar.setTextColor(TFT_CYAN);
        spriteRadar.setCursor(2, 2);
        spriteRadar.printf("%+4d deg/s", (int)lroundf(mw * RPM01_TO_DEG_S));
    } else {
        spriteRadar.setTextSize(2);
        spriteRadar.setTextColor(TFT_DARKGREY);
        spriteRadar.setCursor(24, 120);
        spriteRadar.print("no data");
    }
    spriteRadar.pushSprite(0, RADAR_Y);
}

// Wheel screen: a bar of measured RPM per wheel with a marker for the commanded
// value. A gap between marker and bar shows which wheel is not keeping up.
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

        // A WHOLE number: sampling every 20 ms at 1920 counts per revolution
        // makes one count worth about 1.6 RPM, so a decimal place would be
        // inventing precision that is not there.
        spriteLower.setTextColor(TFT_WHITE);
        spriteLower.setCursor(78, y + 2);
        spriteLower.printf("%4d", (int)lroundf(rows[i].measured / 10.0f));
    }
    spriteLower.pushSprite(0, LOWER_Y);
}

void DisplayManager::panelLink(unsigned rangePercent, unsigned ackLossPercent,
                               unsigned telemLossPermille,
                               uint32_t protoErrors, bool platProtoError) {
    uint32_t sig[5] = { rangePercent, ackLossPercent, telemLossPermille,
                        protoErrors, (uint32_t)platProtoError };
    if (!panelChanged(4, sig, sizeof(sig))) return;

    spriteLower.fillSprite(TFT_BLACK);
    spriteLower.setTextSize(1);

    // Range bar. The mapping is DELIBERATELY non-linear: packet loss does not
    // grow linearly with distance, it shoots up at the edge of range. The bar
    // shows the margin remaining to that edge, not a loss percentage.
    spriteLower.setTextColor(TFT_WHITE);
    spriteLower.setCursor(0, 2);
    spriteLower.print("range");

    const int barX = 46, barY = 2, barW = 78, barH = 9;
    spriteLower.drawRect(barX, barY, barW, barH, TFT_DARKGREY);
    int fill = (int)(barW - 2) * (int)rangePercent / 100;
    uint16_t col = (rangePercent > 60) ? TFT_GREEN
                 : (rangePercent > 25) ? TFT_YELLOW : TFT_RED;
    if (fill > 0) spriteLower.fillRect(barX + 1, barY + 1, fill, barH - 2, col);

    spriteLower.setTextColor(TFT_WHITE);
    spriteLower.setCursor(0, 18);
    spriteLower.printf("no ACK   %u%% of 100", ackLossPercent);
    // One direction only: "<-" is telemetry this pad missed. The other way
    // round is not shown — see the header for why the number would be a lie.
    spriteLower.setCursor(0, 32);
    spriteLower.setTextColor(telemLossPermille ? TFT_YELLOW : TFT_WHITE);
    spriteLower.printf("loss <-  %u/1000", telemLossPermille);

    // Own error count, plus a marker when the PLATFORM reports having received
    // something it could not parse. That flag latches on the platform, so it
    // belongs here rather than in the status bar, where it would sit forever.
    spriteLower.setCursor(0, 46);
    spriteLower.setTextColor((protoErrors || platProtoError) ? TFT_RED : TFT_WHITE);
    spriteLower.printf("errors   %lu%s", (unsigned long)protoErrors,
                       platProtoError ? " +PLAT" : "");

    spriteLower.pushSprite(0, LOWER_Y);
}
