#pragma once
#include <Arduino.h>

// =====================================================================
//  ESP-NOW PROTOCOL — Pad  <->  Mecanum platform
// =====================================================================
//
//  THIS FILE MUST BE IDENTICAL IN BOTH REPOSITORIES. Never edit it in
//  just one of them.
//
//  Messages are dispatched on the FIRST BYTE (msgType); the length is
//  used only to validate before memcpy. Dispatch used to be on sizeof
//  alone — which worked as long as the struct sizes differed, and on an
//  accidental match silently reinterpreted the data as the wrong
//  struct, leaving no trace in any log.
//
//  The protocol version does NOT ride in every frame — it is carried by
//  MSG_HELLO, sent periodically by both sides. ESP-NOW is
//  connectionless: either side may reset at any moment, so HELLO is
//  repeated rather than agreed once at startup.
//
//  The static_asserts below turn an accidental edit to a struct into a
//  COMPILE ERROR — provided both repositories carry this file verbatim.
// =====================================================================

constexpr uint8_t PROTO_VERSION = 3;

// ===== Message types (first byte of every frame) =====
constexpr uint8_t MSG_HELLO       = 1;  // version announcement, both ways
constexpr uint8_t MSG_PAD_CONTROL = 2;  // pad -> platform, control
constexpr uint8_t MSG_TELEMETRY   = 3;  // platform -> pad, telemetry
constexpr uint8_t MSG_SET_PID     = 4;  // pad -> platform, remote tuning

// ===== Device roles (a field in MSG_HELLO) =====
constexpr uint8_t ROLE_PAD      = 1;
constexpr uint8_t ROLE_PLATFORM = 2;

// ===== Bits of the `buttons` field in MSG_PAD_CONTROL =====
// The seesaw pin numbering is sparse (0,1,2,5,6,16), so the buttons are
// repacked densely for transmission. 1 = PRESSED (seesaw reads low when
// pressed; that normalisation happens on the pad).
constexpr uint16_t BTN_L_A      = 1u << 0;
constexpr uint16_t BTN_L_B      = 1u << 1;
constexpr uint16_t BTN_L_X      = 1u << 2;
constexpr uint16_t BTN_L_Y      = 1u << 3;
constexpr uint16_t BTN_L_SELECT = 1u << 4;
constexpr uint16_t BTN_L_START  = 1u << 5;
constexpr uint16_t BTN_R_A      = 1u << 6;
constexpr uint16_t BTN_R_B      = 1u << 7;
constexpr uint16_t BTN_R_X      = 1u << 8;
constexpr uint16_t BTN_R_Y      = 1u << 9;
constexpr uint16_t BTN_R_SELECT = 1u << 10;
constexpr uint16_t BTN_R_START  = 1u << 11;
// bits 12..15 free

// ===== Bits of the `flags` field in MSG_TELEMETRY =====
constexpr uint16_t TFLAG_FAILSAFE     = 1u << 0;  // pad silent, drive cut
constexpr uint16_t TFLAG_HARD_STOPPED = 1u << 1;  // at least one wheel HardStopped
constexpr uint16_t TFLAG_PWM_SAT      = 1u << 2;  // at least one wheel saturated
constexpr uint16_t TFLAG_PROTO_ERROR  = 1u << 3;  // a frame of unknown type/version arrived

// ---------------------------------------------------------------------
//  MSG_HELLO — protocol version announcement. Sent by BOTH sides every
//  ~1 s until the partner's HELLO is seen, then less often.
// ---------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t  msgType;       // = MSG_HELLO
    uint8_t  protoVersion;  // sender's PROTO_VERSION
    uint8_t  role;          // ROLE_PAD / ROLE_PLATFORM
    uint8_t  reserved;
    uint32_t fwBuildId;     // build identifier — a change means different firmware
    uint32_t uptimeMs;      // a drop in this value means the partner reset
} Msg_Hello;
static_assert(sizeof(Msg_Hello) == 12, "Msg_Hello: out of sync with the other repo!");

// ---------------------------------------------------------------------
//  MSG_PAD_CONTROL — pad -> platform, 50 Hz.
//  Raw stick values are NOT transmitted: calibration happens on the pad,
//  and the platform never read them anyway.
// ---------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t  msgType;              // = MSG_PAD_CONTROL
    uint8_t  mode;                 // gear / speed scaling (0 for now)
    uint16_t buttons;              // BTN_* bits, 1 = pressed
    uint32_t timeStamp;            // the pad's millis() — basis of the failsafe
    uint32_t seq;                  // sequence number; gaps = lost frames
    int16_t  axisLX, axisLY;       // left stick, after calibration
    int16_t  axisRX, axisRY;       // right stick, after calibration
    int8_t   rssiFromPlatform;     // how the PAD hears the platform (0 = no data)
    uint8_t  platformLossPermille; // telemetry frames lost, per mille
    uint8_t  reserved[2];
} Msg_PadControl;
static_assert(sizeof(Msg_PadControl) == 24, "Msg_PadControl: out of sync with the other repo!");

// ---------------------------------------------------------------------
//  MSG_TELEMETRY — platform -> pad, sent as a REPLY to every second pad
//  frame, i.e. 25 Hz. There is no telemetry timer: a timer of its own
//  meant a third unsynchronised loop and a round-trip time that jumped
//  by a whole period.
//
//  The current, position and IMU fields are RESERVED — filled with
//  zeros today, because there are no shunts and no IMU yet. Thanks to
//  protocol versioning, populating them later needs no struct change.
//
//  The diagnostic triple target/measured/pwm settles what none of these
//  fields settles alone: a wheel running at the same speed but higher
//  current has mechanical drag; a wheel failing to reach its setpoint
//  despite higher PWM is overloaded or slipping.
//
//  The *Peak / *Min fields are extremes SINCE THE PREVIOUS FRAME,
//  cleared after sending. The cells' current protection trips on a peak
//  lasting milliseconds — a sample every 40 ms would never see it.
// ---------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t  msgType;                 // = MSG_TELEMETRY
    uint8_t  reserved;
    uint16_t flags;                   // TFLAG_* bits
    uint32_t timestamp;               // the platform's millis()
    uint32_t seq;                     // telemetry frame sequence number

    // --- Echo of the last control frame ---
    // The axes are sent back EXACTLY as they arrived: before the dead
    // zone, the scaling and the normalisation. This is not redundancy —
    // it is the only proof that the platform is reading THE RIGHT
    // FIELDS. A matching protocol version does not guarantee that: a
    // two-byte offset still gives a green "OK" and a robot that drives
    // sideways instead of forward.
    //
    // The pad draws a dot from this inside the stick ring: centred =
    // the link is alive and keeping up, trailing = latency, jumping =
    // losses. One pixel instead of three numbers to read while driving.
    int16_t  echoAxisLX, echoAxisLY;
    int16_t  echoAxisRX, echoAxisRY;
    uint32_t echoSeq;                 // seq of the echoed frame — gives RTT in ms

    // FL, FR, RL, RR in 0.1 RPM, in the ROBOT convention: positive = forward
    // for every wheel. The right-hand side inversion (invertDirection) is
    // undone on the platform — the pad must not know about it, or it would
    // have to know the other device's hardware configuration.
    int16_t  targetRPM[4];
    int16_t  measuredRPM[4];
    int16_t  pwm[4];                  // controller output, in PWM units

    int16_t  motorMilliAmp[4];        // RESERVED — per-wheel shunt
    int16_t  motorPeakMilliAmp[4];    // RESERVED — peak since the last frame

    uint16_t cellMilliVolt[2];        // RESERVED — per-cell voltage
    uint16_t cellMinMilliVolt[2];     // RESERVED — sag since the last frame

    int16_t  posXcm, posYcm;          // RESERVED — odometry (approximate!)
    int16_t  headingOdo;              // RESERVED — heading from wheels, 0.1 deg
    int16_t  headingImu;              // RESERVED — heading from gyro, 0.1 deg
    int16_t  pitch, roll;             // RESERVED — 0.1 deg
    int16_t  gyroYawRate;             // RESERVED — 0.1 deg/s from the gyro
    int16_t  accelX, accelY;          // RESERVED — mm/s^2

    // Why a gyro and accelerometer alongside the wheel data: slipping wheels
    // spin faster than the robot travels. Angular rate from the gyro compared
    // with the one derived from the wheels gives a measure of slip, and
    // acceleration cannot be read from the wheels at all — hence accelX/accelY,
    // reserved for the feedback vector.

    int8_t   rssiFromPad;             // how the PLATFORM hears the pad (0 = no data)
    uint8_t  padLossPermille;         // pad frames lost, per mille
    uint16_t motorCtrlTimeUs;         // motorControlTask execution time, us
} Msg_Telemetry;
static_assert(sizeof(Msg_Telemetry) == 94, "Msg_Telemetry: out of sync with the other repo!");

// ---------------------------------------------------------------------
//  MSG_SET_PID — remote retuning. Inherited from the abandoned monitor
//  module; the pad will be the sender.
// ---------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t msgType;      // = MSG_SET_PID
    uint8_t motorIndex;   // 0..3, 0xFF = all wheels
    uint8_t reserved[2];
    float   Kp, Ki, Kd;   // NOTE: the controller's dt is in MILLISECONDS
} Msg_SetPID;
static_assert(sizeof(Msg_SetPID) == 16, "Msg_SetPID: out of sync with the other repo!");

// =====================================================================
//  System constants shared by both devices
//
//  These are not part of the frame format, but they MUST agree on both
//  sides — the pad converts revolutions to speed and scales its gauges
//  from them. They live here because keeping this file identical is an
//  invariant that is actively enforced. On the platform side a
//  static_assert checks them against MAX_RPM.
// =====================================================================

constexpr int   MAX_RPM_TELEMETRY = 900;     // MAX_RPM = 90, in units of 0.1 RPM
constexpr float WHEEL_DIAMETER_M  = 0.060f;  // mecanum wheel diameter

// Telemetry units (0.1 wheel RPM) to m/s:
//   v = (value / 10) * PI * D / 60
constexpr float RPM_TO_MPS = 3.14159265f * WHEEL_DIAMETER_M / 600.0f;

// Geometry needed to turn wheel revolutions into the platform's angular rate.
// Measured centre-of-wheel to centre-of-wheel (2026-08-28).
constexpr float WHEELBASE_M = 0.120f;   // front-rear
constexpr float TRACK_M     = 0.150f;   // left-right (12 cm gap + wheel width)

// In mecanum kinematics the rotation term enters through the sum of the half
// spacings. The linear speed a wheel must have for the platform to rotate at
// angular rate w is w * (lx + ly).
constexpr float MECANUM_LXLY = (WHEELBASE_M + TRACK_M) * 0.5f;

// The rotation term out of mecanumInverse() is in telemetry units (0.1 wheel
// RPM). To degrees per second: the wheel's linear speed divided by (lx + ly)
// gives radians per second, hence the conversion factor.
constexpr float RPM01_TO_DEG_S = RPM_TO_MPS / MECANUM_LXLY * 57.2957795f;

// Inverse mecanum mixing: from four wheels back to the platform's motion.
// These formulas must stay the inverse of MecanumDrive::drive:
//     FL = vy + vx + w      FR = vy - vx - w
//     RL = vy - vx + w      RR = vy + vx - w
// Input in the robot convention (positive = forward); output in the same units
// as the input. Note: vx comes out OPTIMISTIC, because mecanum rollers slip
// sideways by design.
struct MecanumMotion { float vx, vy, omega; };

inline MecanumMotion mecanumInverse(float fl, float fr, float rl, float rr) {
    MecanumMotion m;
    m.vy    = (fl + fr + rl + rr) * 0.25f;
    m.vx    = (fl - fr - rl + rr) * 0.25f;
    m.omega = (fl - fr + rl - rr) * 0.25f;
    return m;
}

// ---------------------------------------------------------------------
//  Build identifier for MSG_HELLO. Derived from the __DATE__/__TIME__ of
//  this translation unit, so every build yields a different value. It is
//  meant to catch "I flashed the old firmware", not to check
//  compatibility — protoVersion does that.
// ---------------------------------------------------------------------
constexpr uint32_t protoFnv1a(const char* s, uint32_t h = 2166136261u) {
    return (*s == '\0') ? h : protoFnv1a(s + 1, (h ^ (uint32_t)(uint8_t)*s) * 16777619u);
}
constexpr uint32_t FW_BUILD_ID = protoFnv1a(__DATE__ __TIME__);
