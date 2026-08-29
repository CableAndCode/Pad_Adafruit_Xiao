#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <SPI.h>
#include <Adafruit_seesaw.h>
#include <TFT_eSPI.h>

#include "parameters.h"
#include "messages.h"
#if __has_include("mac_addresses_private.h")
  #include "mac_addresses_private.h"
#else
  #include "mac_addresses.h"
#endif
#include "joystick_read.h"
#include "DisplayManager.h"

// --- Hardware ---

DisplayManager display;
Adafruit_seesaw ss1, ss2;           // the two I2C gamepads

JoystickReader joystickReaderL(offsetL_X, offsetL_Y, true, true);
JoystickReader joystickReaderR(offsetR_X, offsetR_Y, false, false);

Msg_PadControl message;             // the frame being assembled for transmission

// ---- Protocol state (see the header of messages.h) ----
static volatile uint8_t  platProtoVersion = 0;      // version announced by the platform
static volatile bool     platSeen         = false;  // any HELLO seen at all
static volatile bool     platProtoOk      = false;  // ...and the version matches
static volatile uint32_t lastPlatHelloMs  = 0;      // millis() of the last HELLO

static volatile uint32_t protoErrorCount  = 0;      // frames of unknown type/length
static volatile uint8_t  lastUnknownType  = 0;      // shown in the status bar
static volatile int      lastUnknownLen   = 0;

// Protocol version announcements. Frequent while the platform has not been seen,
// rare once it has: version agreement does not change during operation.
constexpr uint32_t HELLO_INTERVAL_SEARCH_MS = 1000;
constexpr uint32_t HELLO_INTERVAL_IDLE_MS   = 5000;

// ---- Most recent telemetry ----
// Written in the receive callback (WiFi task), read by the display task.
static SemaphoreHandle_t telemetryMutex = nullptr;
static Msg_Telemetry     lastTelemetry;
static volatile uint32_t lastTelemetryMs = 0;
static volatile uint32_t telemSeqLast    = 0;
static volatile bool     telemEverSeen   = false;  // permanent marker
static volatile uint32_t telemRecvCount  = 0;      // window counter, cleared periodically
static volatile uint32_t telemMissCount  = 0;

// Telemetry arrives at 25 Hz (a reply to every second pad frame), so silence
// longer than this means ten frames lost in a row. This replaced detection via
// HELLO, which at a 5 s interval was uselessly slow — HELLO now only covers the
// case of "we have never seen the platform at all".
constexpr uint32_t TELEM_TIMEOUT_MS = 400;

// Silence longer than this means the platform is gone. The threshold is high
// because in this case the only signal we ever had from it is HELLO every 5 s.
// Once telemetry has arrived even once, TELEM_TIMEOUT_MS takes over and the
// detection drops to a fraction of a second.
constexpr uint32_t PLAT_HELLO_TIMEOUT_MS = 15000;

// Send time of the frame with a given number, used to compute the round trip
// from echoSeq. At 50 Hz, 64 slots is ~1.3 s of history — far more than any
// sensible latency.
constexpr uint32_t RTT_RING = 64;
static volatile uint32_t sendMs[RTT_RING] = { 0 };
static volatile uint32_t lastRttMs = 0;
static volatile uint32_t handshakeAtMs = 0;  // when the versions first matched

// The version line is a STARTUP MESSAGE, not a permanent element — it goes away
// after a few seconds and frees the band for signal strength and warnings.
// "the versions agree" is needed once; what changes while driving needs the
// space permanently.
constexpr uint32_t LINK_BANNER_MS = 4000;

// Repacks the buttons from the sparse seesaw pin numbering (0,1,2,5,6,16) into
// the protocol's dense bits. Normalisation happens here too: seesaw reads LOW
// when a button is pressed, and 1 = PRESSED goes on air, so the receiver need
// know nothing about pull-ups.
static uint16_t packButtons(uint32_t rawL, uint32_t rawR) {
    uint16_t b = 0;
    if (!(rawL & (1UL << BUTTON_A)))      b |= BTN_L_A;
    if (!(rawL & (1UL << BUTTON_B)))      b |= BTN_L_B;
    if (!(rawL & (1UL << BUTTON_X)))      b |= BTN_L_X;
    if (!(rawL & (1UL << BUTTON_Y)))      b |= BTN_L_Y;
    if (!(rawL & (1UL << BUTTON_SELECT))) b |= BTN_L_SELECT;
    if (!(rawL & (1UL << BUTTON_START)))  b |= BTN_L_START;
    if (!(rawR & (1UL << BUTTON_A)))      b |= BTN_R_A;
    if (!(rawR & (1UL << BUTTON_B)))      b |= BTN_R_B;
    if (!(rawR & (1UL << BUTTON_X)))      b |= BTN_R_X;
    if (!(rawR & (1UL << BUTTON_Y)))      b |= BTN_R_Y;
    if (!(rawR & (1UL << BUTTON_SELECT))) b |= BTN_R_SELECT;
    if (!(rawR & (1UL << BUTTON_START)))  b |= BTN_R_START;
    return b;
}

// Receive callback. Dispatch is on the first byte, with the length used as
// validation (see the header of messages.h).
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    // Only the platform may talk to us. ESP-NOW raises the receive callback for
    // ANY sender on the channel — registering a peer governs sending, not
    // receiving. OnDataSent already filters this way; the receive path did not,
    // which meant a foreign device could have fed this pad telemetry.
    if (mac == nullptr || memcmp(mac, macPlatformMecanum, 6) != 0) return;

    if (len < 1) return;
    const uint8_t type = incomingData[0];

    switch (type) {
    case MSG_HELLO: {
        // Bytes 0 and 1 are frozen across every version of this protocol:
        // message type, then protocol version. The version is read BEFORE the
        // length check ON PURPOSE. A bump that changes the SIZE of Msg_Hello
        // makes the two sides reject each other's HELLO on its length, and the
        // version would then be unreadable in the one case it matters —
        // v3 -> v4 was exactly such a bump, 12 bytes down to 8. Without this,
        // meeting an older platform showed "?TYPE 1  LEN 12" instead of
        // "PLAT v3  BAD VER".
        //
        // platProtoOk is therefore set before the role check as well. That is
        // safe because the MAC filter above has already let through nobody but
        // the platform; do not "tidy" this by moving it back below the role
        // test.
        if (len >= 2) {
            platSeen         = true;
            platProtoVersion = incomingData[1];
            platProtoOk      = (incomingData[1] == PROTO_VERSION);
        }
        // A frame of the wrong length still falls through to protoErrorCount.
        // "Unreadable frame" and "version mismatch" are separate states and
        // both have to stay visible. lastPlatHelloMs and handshakeAtMs are
        // updated only for a FULLY decoded frame — they mean "a partner we can
        // actually talk to", which a mismatched HELLO is not.
        if (len != (int)sizeof(Msg_Hello)) break;
        Msg_Hello hello;
        memcpy(&hello, incomingData, sizeof(hello));
        if (hello.role != ROLE_PLATFORM) break;
        lastPlatHelloMs  = millis();
        if (platProtoOk && handshakeAtMs == 0) handshakeAtMs = millis();
        return;
    }

    case MSG_TELEMETRY:
        if (len != (int)sizeof(Msg_Telemetry)) break;
        if (telemetryMutex && xSemaphoreTake(telemetryMutex, 0) == pdTRUE) {
            memcpy(&lastTelemetry, incomingData, sizeof(Msg_Telemetry));
            lastTelemetryMs = millis();
            // Gaps in the numbering = telemetry frames lost on the way.
            if (telemEverSeen && lastTelemetry.seq > telemSeqLast + 1) {
                telemMissCount += lastTelemetry.seq - telemSeqLast - 1;
            }
            telemSeqLast  = lastTelemetry.seq;
            telemRecvCount++;
            telemEverSeen = true;

            // Round trip: from sending the frame numbered echoSeq to the moment
            // it came back in telemetry. Measured on ONE clock, so any drift
            // between the two devices' clocks is irrelevant here.
            uint32_t es = lastTelemetry.echoSeq;
            if (es > 0 && (totalMessages - es) < RTT_RING) {
                lastRttMs = millis() - sendMs[es % RTT_RING];
            }
            xSemaphoreGive(telemetryMutex);
        }
        return;

    default:
        break;
    }

    protoErrorCount++;
    lastUnknownType = type;
    lastUnknownLen  = len;
}

// Announcing the protocol version — repeated, because ESP-NOW has no notion of
// a session and the platform may reset at any moment.
void TaskHello(void *pvParameters) {
    (void)pvParameters;
    while (1) {
        Msg_Hello hello = {};
        hello.msgType      = MSG_HELLO;
        hello.protoVersion = PROTO_VERSION;
        hello.role         = ROLE_PAD;
        hello.fwBuildId    = FW_BUILD_ID;
        esp_now_send(macPlatformMecanum, (uint8_t *)&hello, sizeof(hello));
        vTaskDelay(pdMS_TO_TICKS(platProtoOk ? HELLO_INTERVAL_IDLE_MS
                                             : HELLO_INTERVAL_SEARCH_MS));
    }
}

// --- Send callback: delivery status of transmitted ESP-NOW frames ---
// A window over the last ACK_WINDOW transmissions. A total since boot only ever
// grows and says nothing; what matters is the loss rate NOW, because that is
// what reflects range.
constexpr int ACK_WINDOW = 100;
static volatile uint8_t ackFail[ACK_WINDOW] = { 0 };
static volatile int     ackIndex = 0;
static volatile int     ackFailCount = 0;      // number of ones in the window

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (memcmp(mac_addr, macPlatformMecanum, 6) != 0) return;

    const bool failed = (status != ESP_NOW_SEND_SUCCESS);
    ackFailCount -= ackFail[ackIndex];
    ackFail[ackIndex] = failed ? 1 : 0;
    ackFailCount += ackFail[ackIndex];
    ackIndex = (ackIndex + 1) % ACK_WINDOW;
}

// Loss rate mapped onto a "range" figure. The mapping is DELIBERATELY
// non-linear: at the edge of range, losses shoot up, so a few per cent of lost
// packets already means almost no margin left. Anchor points: 0 % loss = 100
// range, 5 % = 50, 10 % = 20, 20 % and above = 0.
static unsigned rangeFromLoss(unsigned lossPercent) {
    if (lossPercent == 0)  return 100;
    if (lossPercent <= 5)  return 100 - (lossPercent * 50) / 5;
    if (lossPercent <= 10) return 50  - ((lossPercent - 5) * 30) / 5;
    if (lossPercent <= 20) return 20  - ((lossPercent - 10) * 20) / 10;
    return 0;
}

// --- TASK 1: reading the gamepads ---
// Every 20 ms: read and calibrate the sticks, read the buttons, and update the
// shared message structure under its mutex.
void TaskGamepads(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // 50 Hz
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        int localL_Joystick_raw_x = ss1.analogRead(14);
        int localL_Joystick_raw_y = ss1.analogRead(15);
        int localR_Joystick_raw_x = ss2.analogRead(14);
        int localR_Joystick_raw_y = ss2.analogRead(15);

        int localL_Joystick_x = joystickReaderL.getCorrectedValueX(localL_Joystick_raw_x);
        int localL_Joystick_y = joystickReaderL.getCorrectedValueY(localL_Joystick_raw_y);
        int localR_Joystick_x = joystickReaderR.getCorrectedValueX(localR_Joystick_raw_x);
        int localR_Joystick_y = joystickReaderR.getCorrectedValueY(localR_Joystick_raw_y);

        int localL_Joystick_buttons_message = ss1.digitalReadBulk(button_mask);
        int localR_Joystick_buttons_message = ss2.digitalReadBulk(button_mask2);

        // Critical section: update the shared message.
        // Raw values no longer go on air — calibration happens here, and the
        // platform never read them anyway.
        xSemaphoreTake(messageMutex, portMAX_DELAY);
        message.msgType = MSG_PAD_CONTROL;

        message.axisLX = localL_Joystick_x;
        message.axisLY = localL_Joystick_y;
        message.axisRX = localR_Joystick_x;
        message.axisRY = localR_Joystick_y;

        message.buttons = packButtons((uint32_t)localL_Joystick_buttons_message,
                                      (uint32_t)localR_Joystick_buttons_message);
        xSemaphoreGive(messageMutex);

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// --- TASK 2: transmitting over ESP-NOW ---
// Every 20 ms: copy the latest gamepad state and send it to the platform.
void TaskESPNow(void *pvParameters) {
    (void)pvParameters;
    // 50 Hz, matching the stick sampling rate. At 20 Hz, three independent
    // 50 ms loops (transmit, telemetry, drawing) drifted in phase against each
    // other and the interval between echo-dot updates jumped between 0 and
    // 100 ms — that, rather than the rate itself, is what looked like stutter.
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // 50 Hz
    TickType_t xLastWakeTime = xTaskGetTickCount();
    Msg_PadControl localMsg;

    while (1) {
        xSemaphoreTake(messageMutex, portMAX_DELAY);
        totalMessages++;
        message.seq = totalMessages;
        sendMs[totalMessages % RTT_RING] = millis();
        memcpy(&localMsg, &message, sizeof(Msg_PadControl));
        xSemaphoreGive(messageMutex);

        // The platform is the only recipient. Transmitting to the abandoned
        // monitor's address as well cost MAC-layer retries (unicast without an
        // ACK) immediately before the frame that actually matters — and that
        // was visible as a jittering echo dot.
        esp_now_send(macPlatformMecanum, (uint8_t *)&localMsg, sizeof(Msg_PadControl));

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// The splash screen gives way after the handshake, and — if there is no
// platform at all — after this long at the latest, so the pad does not hang on
// it indefinitely.
constexpr uint32_t SPLASH_MAX_MS = 6000;

// Telemetry frame loss over a window, not since boot — otherwise, after a few
// minutes of driving, individual lost frames would dissolve into the average.
static unsigned telemLossPermille = 0;

// Status bar: warnings take priority, and when everything is fine it shows what
// changes while driving — the round trip and the frame loss.
//
// telFlags carries the platform's own view of itself (TFLAG_*). It is only
// trusted while the telemetry is fresh — a stale frame must not keep showing a
// failsafe that has long since cleared.
static void drawLinkStatusLine(uint32_t rttMs, uint16_t telFlags, int y) {
    char text[24];
    uint16_t color = TFT_GREEN;
    uint32_t nowMs = millis();

    static uint32_t lossWindowMs = 0;
    if (nowMs - lossWindowMs > 2000) {
        uint32_t tot = telemRecvCount + telemMissCount;
        telemLossPermille = tot ? (unsigned)((telemMissCount * 1000u) / tot) : 0;
        telemRecvCount = 0;
        telemMissCount = 0;
        lossWindowMs = nowMs;
    }

    bool echoFresh = telemEverSeen && ((nowMs - lastTelemetryMs) < TELEM_TIMEOUT_MS);

    if (!platSeen) {
        snprintf(text, sizeof(text), "PLAT --  searching");
        color = TFT_YELLOW;
    } else if (!platProtoOk) {
        snprintf(text, sizeof(text), "PLAT v%u  BAD VER",
                 (unsigned)platProtoVersion);
        color = TFT_RED;
    } else if (telemEverSeen && !echoFresh) {
        snprintf(text, sizeof(text), "PLAT LOST");
        color = TFT_RED;
    } else if (!telemEverSeen &&
               (nowMs - lastPlatHelloMs) > PLAT_HELLO_TIMEOUT_MS) {
        snprintf(text, sizeof(text), "PLAT LOST");
        color = TFT_RED;
    } else if (echoFresh && (telFlags & TFLAG_FAILSAFE)) {
        // The platform cut its drive because IT cannot hear US. This is not the
        // same state as "we cannot hear the platform" above, and until now the
        // pad could not tell them apart: on an asymmetric link the pad happily
        // showed a healthy RTT while the robot refused to move.
        snprintf(text, sizeof(text), "DRIVE CUT");
        color = TFT_RED;
    } else if (protoErrorCount > 0) {
        // Deliberately BELOW the safety states. This counter never resets, so
        // at the head of the chain a single unparseable frame pinned the bar to
        // "?TYPE ..." for the rest of the session and hid PLAT LOST,
        // PLAT v.. BAD VER and DRIVE CUT behind it.
        //
        // Time-limiting it instead would not have been enough: on a version
        // mismatch the partner's HELLO keeps arriving every second and keeps
        // failing the length check, so any "recent bad frame" window would be
        // refreshed forever and would still cover BAD VER — which is the exact
        // case this bar exists for. Order is the fix; a timer is not.
        //
        // The count itself keeps rising and stays on the link panel.
        snprintf(text, sizeof(text), "?TYPE %u  LEN %d",
                 (unsigned)lastUnknownType, lastUnknownLen);
        color = TFT_RED;
    } else if ((nowMs - handshakeAtMs) < LINK_BANNER_MS) {
        snprintf(text, sizeof(text), "PLAT v%u  OK", (unsigned)platProtoVersion);
        color = TFT_GREEN;
    } else {
        snprintf(text, sizeof(text), "RTT %ums  L%u",
                 (unsigned)rttMs, telemLossPermille);
        color = TFT_CYAN;
    }

    // Redraw only on an actual change, so the bar does not flicker.
    static char lastText[24] = { 1 };
    static uint16_t lastColor = 0;
    static int lastY = -1;
    if (strcmp(text, lastText) != 0 || color != lastColor || y != lastY) {
        display.updateLinkStatus(text, color, y);
        strncpy(lastText, text, sizeof(lastText));
        lastColor = color;
        lastY = y;
    }
}

// --- Screens ---
// Switching is LOCAL to the pad: which view the operator is looking at is no
// business of the platform's, so it costs not a single bit on air.
enum Screen : uint8_t {
    SCR_DRIVE = 0,   // velocity vectors — commanded and actual
    SCR_WHEELS,      // the four wheels individually
    SCR_LINK,        // link quality in numbers
    SCR_BUTTONS,     // button test
    SCR_COUNT
};

// The left pad's SELECT cycles forwards, the right pad's backwards.
// START is deliberately LEFT FREE — it is reserved for a future emergency stop,
// which must not collide with navigation.
static uint8_t currentScreen = SCR_DRIVE;

// --- TASK 3: drawing the TFT screen ---
// Every 20 ms: read the sticks, buttons and telemetry, then draw the active
// screen. Each panel decides for itself whether its data changed.
void TaskTFTScreen(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // 50 Hz
    TickType_t xLastWakeTime = xTaskGetTickCount();

    bool splashActive = true;
    bool prevSelL = false, prevSelR = false;
    uint32_t bootMs = millis();

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        xSemaphoreTake(messageMutex, portMAX_DELAY);
        int lx = message.axisLX;
        int ly = message.axisLY;
        int rx = message.axisRX;
        int ry = message.axisRY;
        uint16_t btn = message.buttons;
        xSemaphoreGive(messageMutex);

        const bool L[6] = { (bool)(btn & BTN_L_A), (bool)(btn & BTN_L_B),
                            (bool)(btn & BTN_L_X), (bool)(btn & BTN_L_Y),
                            (bool)(btn & BTN_L_SELECT), (bool)(btn & BTN_L_START) };
        const bool R[6] = { (bool)(btn & BTN_R_A), (bool)(btn & BTN_R_B),
                            (bool)(btn & BTN_R_X), (bool)(btn & BTN_R_Y),
                            (bool)(btn & BTN_R_SELECT), (bool)(btn & BTN_R_START) };

        // Copy the telemetry under the mutex, then work on our own copy, so the
        // lock is not held for the duration of the drawing.
        Msg_Telemetry tel;
        bool echoValid = false;
        uint32_t rtt = 0;
        if (xSemaphoreTake(telemetryMutex, portMAX_DELAY) == pdTRUE) {
            memcpy(&tel, &lastTelemetry, sizeof(tel));
            echoValid = telemEverSeen &&
                        ((millis() - lastTelemetryMs) < TELEM_TIMEOUT_MS);
            rtt = lastRttMs;
            xSemaphoreGive(telemetryMutex);
        }

        // --- splash: gives way after the handshake, or after SPLASH_MAX_MS ---
        if (splashActive) {
            bool ready = platProtoOk && (millis() - handshakeAtMs) > 1500;
            if (ready || (millis() - bootMs) > SPLASH_MAX_MS ||
                L[4] || R[4]) {
                splashActive = false;
                display.invalidate();
                display.clearAll();
            } else {
                static char splashStatus[24] = { 1 };
                char st[24];
                if (!platSeen)          snprintf(st, sizeof(st), "searching platform");
                else if (!platProtoOk)  snprintf(st, sizeof(st), "bad version: v%u",
                                                 (unsigned)platProtoVersion);
                else                    snprintf(st, sizeof(st), "connected");
                if (strcmp(st, splashStatus) != 0) {
                    display.showSplash(PROTO_VERSION, FW_BUILD_ID, st);
                    strncpy(splashStatus, st, sizeof(splashStatus));
                }
                continue;
            }
        }

        // --- navigation: rising edge, so holding the button does not scroll ---
        if (L[4] && !prevSelL) {
            currentScreen = (currentScreen + 1) % SCR_COUNT;
            display.invalidate();
            display.clearAll();
        }
        if (R[4] && !prevSelR) {
            currentScreen = (currentScreen + SCR_COUNT - 1) % SCR_COUNT;
            display.invalidate();
            display.clearAll();
        }
        prevSelL = L[4];
        prevSelR = R[4];

        // The drive screen has a layout of its own: status bar on top, radar
        // filling the rest. Stick rings are redundant there — the radar proves
        // the same thing and more.
        const bool radarLayout = (currentScreen == SCR_DRIVE);
        drawLinkStatusLine(rtt, tel.flags, radarLayout ? 0 : 65);
        if (!radarLayout) {
            display.updateJoystick(lx, ly, rx, ry, echoValid,
                                   tel.echoAxisLX, tel.echoAxisLY,
                                   tel.echoAxisRX, tel.echoAxisRY);
        }

        switch (currentScreen) {
        case SCR_DRIVE: {
            // Vectors reconstructed from the four wheels' revolutions.
            MecanumMotion t = mecanumInverse(tel.targetRPM[0], tel.targetRPM[1],
                                             tel.targetRPM[2], tel.targetRPM[3]);
            MecanumMotion m = mecanumInverse(tel.measuredRPM[0], tel.measuredRPM[1],
                                             tel.measuredRPM[2], tel.measuredRPM[3]);
            display.panelRadar(t.vx, t.vy, t.omega, m.vx, m.vy, m.omega, echoValid);
            break;
        }
        case SCR_WHEELS: {
            WheelRow rows[4];
            for (int i = 0; i < 4; i++) {
                rows[i].target   = echoValid ? tel.targetRPM[i]   : 0;
                rows[i].measured = echoValid ? tel.measuredRPM[i] : 0;
                rows[i].pwm      = echoValid ? tel.pwm[i]         : 0;
            }
            display.panelWheels(rows);
            break;
        }
        case SCR_LINK: {
            unsigned ackLoss = (unsigned)ackFailCount * 100u / ACK_WINDOW;
            display.panelLink(rangeFromLoss(ackLoss), ackLoss,
                              telemLossPermille,
                              protoErrorCount,
                              echoValid && (tel.flags & TFLAG_PROTO_ERROR));
            break;
        }
        case SCR_BUTTONS:
            display.panelButtons(L, R);
            break;
        }
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin(5, 6);               // I2C: SDA = 5, SCL = 6
    display.begin();

    if (!ss1.begin(GAMEPAD1_ADDR) || !ss2.begin(GAMEPAD2_ADDR)) {
        Serial.println("Gamepad not found!");
        while (1) delay(100);
    }
    Serial.println("Gamepad OK!");

    ss1.pinModeBulk(button_mask, INPUT_PULLUP);
    ss1.setGPIOInterrupts(button_mask, 1);
    ss2.pinModeBulk(button_mask2, INPUT_PULLUP);
    ss2.setGPIOInterrupts(button_mask2, 1);

    // Joystick centre readings, taken once with the sticks at rest. Each device
    // calibrates itself instead of trusting a nominal centre.
    offsetL_X = ss1.analogRead(14);
    offsetL_Y = ss1.analogRead(15);
    offsetR_X = ss2.analogRead(14);
    offsetR_Y = ss2.analogRead(15);
    joystickReaderL.setOffset(offsetL_X, offsetL_Y);
    joystickReaderR.setOffset(offsetR_X, offsetR_Y);

    // Mutexes before ESP-NOW — see the comment at register_recv_cb.
    messageMutex   = xSemaphoreCreateMutex();
    telemetryMutex = xSemaphoreCreateMutex();
    if (messageMutex == NULL || telemetryMutex == NULL) {
        Serial.println("Failed to create mutex!");
        while (1) delay(100);
    }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return;
    }
    esp_now_register_send_cb(OnDataSent);
    // Registering the receive callback MUST come after the mutexes exist: the
    // WiFi task can call OnDataRecv immediately, while setup() is still running.
    esp_now_register_recv_cb(OnDataRecv);

    // The mecanum platform is the pad's only peer.
    memcpy(peerInfo.peer_addr, macPlatformMecanum, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer (platform)");
        return;
    }

    xTaskCreate(TaskGamepads,  "Gamepads",   2048, NULL, 1, NULL);
    xTaskCreate(TaskESPNow,    "ESPNowSend", 2048, NULL, 1, NULL);
    xTaskCreate(TaskTFTScreen, "TFTScreen",  4096, NULL, 1, NULL);
    xTaskCreate(TaskHello,     "Hello",      2048, NULL, 1, NULL);
}

void loop() {
    // Everything runs in the FreeRTOS tasks.
}
