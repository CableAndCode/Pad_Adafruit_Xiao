#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <TFT_eSPI.h>
#include "messages.h"

// The screen has TWO layouts.
//
// Standard layout (wheels, link, buttons):
//   y   0..63   stick rings with their echo dots
//   y  65..88   link status bar
//   y  90..159  lower panel — contents depend on the selected screen
//
// Radar layout (the drive screen):
//   y   0..23   link status bar
//   y  24..159  radar filling everything else
//
// The stick rings are dropped on the radar screen on purpose: the echo dot
// proved that the platform was reading the right fields, and the radar proves
// MORE — it is drawn from wheel revolutions, so it confirms the whole path
// including the kinematics and the controller.
//
// The lower panel is ONE sprite shared by every view. The two former button
// sprites (64x70 each) took exactly as much memory as one 128x70, so merging
// them freed nothing by itself — the point is different: every new screen draws
// into the same buffer instead of allocating its own. Three additional panels
// in separate sprites would have cost over 50 kB.
constexpr int LOWER_Y = 90;
constexpr int LOWER_W = 128;
constexpr int LOWER_H = 70;

constexpr int RADAR_Y = 24;
constexpr int RADAR_W = 128;
constexpr int RADAR_H = 136;

struct WheelRow {
    int16_t target;    ///< 0.1 RPM, robot convention
    int16_t measured;  ///< 0.1 RPM, robot convention
    int16_t pwm;       ///< controller output, PWM units
};

class DisplayManager {
public:
    DisplayManager();
    void begin();

    /// Splash screen — drawn straight to the display, without sprites.
    void showSplash(uint8_t protoVersion, uint32_t buildId, const char* statusLine);

    /// Clears the whole screen (used when leaving the splash or switching views).
    void clearAll();

    /// Forces a full redraw on the next call to every panel.
    void invalidate();

    void updateJoystick(int lx, int ly, int rx, int ry,
                        bool echoValid, int elx, int ely, int erx, int ery);
    void updateLinkStatus(const char* text, uint16_t color, int y);

    // --- Lower panels; each redraws only when its data changed ---
    void panelButtons(const bool L[6], const bool R[6]);
    void panelWheels(const WheelRow rows[4]);

    /// RTT is deliberately absent here — it lives in the status bar, where it
    /// is visible on every screen.
    ///
    /// Loss is shown for ONE direction only: telemLossPermille, what this pad
    /// missed coming down. It is computed here, from gaps in the telemetry
    /// seq — nothing on the wire carries it.
    ///
    /// The upward direction is gone as of protocol v4. The platform used to
    /// send it, over a window of two frames (telemetry answers every second
    /// pad frame), so it could only ever read 0, 500 or 1000 — and the uint8_t
    /// field clipped anything above 255 anyway. A number with three possible
    /// values, two of them off the scale, is not a measurement.
    ///
    /// It earns its place back when the link has a real range to fail over.
    /// That means a wider window on the platform and a field added back to
    /// messages.h in both repositories — which is what bumping PROTO_VERSION
    /// is for. Until then the echo dot in the stick ring says more about the
    /// link than this ever did.
    void panelLink(unsigned rangePercent, unsigned ackLossPercent,
                   unsigned telemLossPermille,
                   uint32_t protoErrors, bool platProtoError);

    /// Full-screen radar: travel vectors and rotation arcs.
    void panelRadar(float tvx, float tvy, float tw,
                    float mvx, float mvy, float mw, bool valid);

private:
    TFT_eSPI    tft;
    TFT_eSprite spriteJoystick_L;
    TFT_eSprite spriteJoystick_R;
    TFT_eSprite spriteStatus;
    TFT_eSprite spriteLower;
    TFT_eSprite spriteRadar;

    int  lastLx, lastLy, lastRx, lastRy;
    int  lastElx, lastEly, lastErx, lastEry;
    bool lastEchoValid;

    // Panel signatures — compared byte for byte, so the lower half of the
    // screen is not redrawn without reason.
    uint8_t lastPanelId;
    uint8_t lastPanelData[32];

    bool panelChanged(uint8_t id, const void* data, size_t len);
    void fillDiamond(TFT_eSprite& sprite, int cx, int cy, int size, uint16_t color);
    void drawVector(TFT_eSprite& sp, int cx, int cy, int r,
                    float vx, float vy, uint16_t color);
    void drawSpinArc(TFT_eSprite& sp, int cx, int cy, int r,
                     float frac, uint16_t color);
};

#endif // DISPLAY_MANAGER_H
