#pragma once
#include <Arduino.h>

// =====================================================================
//  PROTOKÓŁ ESP-NOW — Pad  <->  Platforma mecanum
// =====================================================================
//
//  TEN PLIK MUSI BYĆ IDENTYCZNY W OBU REPO. Nie edytuj go w jednym.
//
//  Rozpoznawanie wiadomości odbywa się po PIERWSZYM BAJCIE (msgType),
//  a długość służy wyłącznie do walidacji przed memcpy. Wcześniej typ
//  był rozpoznawany po samym sizeof — działało, dopóki rozmiary były
//  różne, a przy przypadkowej zgodności dawało interpretację danych
//  jako niewłaściwej struktury, po cichu i bez śladu w logach.
//
//  Wersja protokołu NIE leci w każdej ramce — wymieniają ją MSG_HELLO
//  nadawane okresowo przez obie strony. ESP-NOW jest bezpołączeniowy:
//  każda ze stron może się zresetować w dowolnej chwili, więc HELLO są
//  powtarzane, a nie uzgadniane raz na starcie.
//
//  static_assert niżej zamienia przypadkową edycję struktury w BŁĄD
//  KOMPILACJI — pod warunkiem, że oba repo mają ten plik identyczny.
// =====================================================================

constexpr uint8_t PROTO_VERSION = 2;

// ===== Typy wiadomości (pierwszy bajt każdej ramki) =====
constexpr uint8_t MSG_HELLO       = 1;  // ogłoszenie wersji, w obie strony
constexpr uint8_t MSG_PAD_CONTROL = 2;  // Pad -> Platforma, sterowanie
constexpr uint8_t MSG_TELEMETRY   = 3;  // Platforma -> Pad, telemetria
constexpr uint8_t MSG_SET_PID     = 4;  // Pad -> Platforma, zdalne nastawy

// ===== Role urządzeń (pole w MSG_HELLO) =====
constexpr uint8_t ROLE_PAD      = 1;
constexpr uint8_t ROLE_PLATFORM = 2;

// ===== Bity pola `buttons` w MSG_PAD_CONTROL =====
// Numeracja pinów seesaw jest rzadka (0,1,2,5,6,16), więc do wysyłki
// przyciski są przepakowane gęsto. 1 = WCIŚNIĘTY (seesaw daje stan
// niski przy wciśnięciu — normalizacja odbywa się po stronie Pada).
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
// bity 12..15 wolne

// ===== Bity pola `flags` w MSG_TELEMETRY =====
constexpr uint16_t TFLAG_FAILSAFE     = 1u << 0;  // cisza z Pada, napęd odcięty
constexpr uint16_t TFLAG_HARD_STOPPED = 1u << 1;  // co najmniej jedno koło w HardStopped
constexpr uint16_t TFLAG_PWM_SAT      = 1u << 2;  // co najmniej jedno koło w nasyceniu
constexpr uint16_t TFLAG_PROTO_ERROR  = 1u << 3;  // odebrano ramkę nieznanego typu/wersji

// ---------------------------------------------------------------------
//  MSG_HELLO — ogłoszenie wersji protokołu. Nadawane przez OBIE strony
//  co ~1 s dopóki nie zobaczą HELLO partnera, potem rzadziej.
// ---------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t  msgType;       // = MSG_HELLO
    uint8_t  protoVersion;  // = PROTO_VERSION nadawcy
    uint8_t  role;          // ROLE_PAD / ROLE_PLATFORM
    uint8_t  reserved;
    uint32_t fwBuildId;     // identyfikator builda — zmiana = inne firmware
    uint32_t uptimeMs;      // spadek wartości = partner się zresetował
} Msg_Hello;
static_assert(sizeof(Msg_Hello) == 12, "Msg_Hello: rozjazd z drugim repo!");

// ---------------------------------------------------------------------
//  MSG_PAD_CONTROL — Pad -> Platforma, 20 Hz.
//  Surowe wartości drążków NIE są przesyłane: kalibracja odbywa się na
//  Padzie, a platforma i tak ich nigdy nie czytała.
// ---------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t  msgType;              // = MSG_PAD_CONTROL
    uint8_t  mode;                 // bieg / skalowanie prędkości (na razie 0)
    uint16_t buttons;              // bity BTN_*, 1 = wciśnięty
    uint32_t timeStamp;            // millis() Pada — podstawa failsafe
    uint32_t seq;                  // numer kolejny; luki = zgubione ramki
    int16_t  axisLX, axisLY;       // lewy drążek po kalibracji
    int16_t  axisRX, axisRY;       // prawy drążek po kalibracji
    int8_t   rssiFromPlatform;     // jak PAD słyszy platformę (0 = brak danych)
    uint8_t  platformLossPermille; // ile ramek telemetrii zgubione, ‰
    uint8_t  reserved[2];
} Msg_PadControl;
static_assert(sizeof(Msg_PadControl) == 24, "Msg_PadControl: rozjazd z drugim repo!");

// ---------------------------------------------------------------------
//  MSG_TELEMETRY — Platforma -> Pad, 20 Hz.
//
//  Pola prądu, pozycji i IMU są ZAREZERWOWANE — dziś wypełniane zerami,
//  bo nie ma jeszcze boczników ani IMU. Dzięki wersjonowaniu protokołu
//  ich późniejsze wypełnienie nie wymaga zmiany struktury.
//
//  Trójka diagnostyczna target/measured/pwm rozstrzyga to, czego żadne
//  z tych pól nie rozstrzyga osobno: koło o tej samej prędkości, ale
//  wyższym prądzie ma opory mechaniczne; koło nie dociągające do zadanej
//  mimo wyższego PWM jest przeciążone albo się ślizga.
//
//  Pola *Peak / *Min to wartości skrajne OD POPRZEDNIEJ RAMKI, zerowane
//  po wysłaniu. Zabezpieczenie prądowe ogniw wywala się na szczycie
//  trwającym milisekundy — próbka co 50 ms takiego szczytu nie zobaczy.
// ---------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t  msgType;                 // = MSG_TELEMETRY
    uint8_t  reserved;
    uint16_t flags;                   // bity TFLAG_*
    uint32_t timestamp;               // millis() platformy
    uint32_t seq;                     // numer kolejny ramki telemetrii

    int16_t  targetRPM[4];            // FL, FR, RL, RR — 0,1 RPM
    int16_t  measuredRPM[4];          // 0,1 RPM
    int16_t  pwm[4];                  // wyjście regulatora, jednostki PWM

    int16_t  motorMilliAmp[4];        // ZAREZERWOWANE — bocznik per koło
    int16_t  motorPeakMilliAmp[4];    // ZAREZERWOWANE — szczyt od ostatniej ramki

    uint16_t cellMilliVolt[2];        // ZAREZERWOWANE — napięcie per ogniwo
    uint16_t cellMinMilliVolt[2];     // ZAREZERWOWANE — zapaść od ostatniej ramki

    int16_t  posXcm, posYcm;          // ZAREZERWOWANE — odometria (orientacyjna!)
    int16_t  headingOdo;              // ZAREZERWOWANE — kurs z kół, 0,1°
    int16_t  headingImu;              // ZAREZERWOWANE — kurs z żyroskopu, 0,1°
    int16_t  pitch, roll;             // ZAREZERWOWANE — 0,1°

    int8_t   rssiFromPad;             // jak PLATFORMA słyszy Pada (0 = brak danych)
    uint8_t  padLossPermille;         // ile ramek z Pada zgubione, ‰
    uint16_t motorCtrlTimeUs;         // czas motorControlTask, µs
} Msg_Telemetry;
static_assert(sizeof(Msg_Telemetry) == 76, "Msg_Telemetry: rozjazd z drugim repo!");

// ---------------------------------------------------------------------
//  MSG_SET_PID — zdalna zmiana nastaw. Dziedzictwo po porzuconym
//  monitorze; docelowo nadawane przez Pada.
// ---------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t msgType;      // = MSG_SET_PID
    uint8_t motorIndex;   // 0..3, 0xFF = wszystkie koła
    uint8_t reserved[2];
    float   Kp, Ki, Kd;   // UWAGA: dt regulatora jest w MILISEKUNDACH
} Msg_SetPID;
static_assert(sizeof(Msg_SetPID) == 16, "Msg_SetPID: rozjazd z drugim repo!");

// ---------------------------------------------------------------------
//  Identyfikator builda do MSG_HELLO. Liczony z __DATE__/__TIME__ tej
//  jednostki kompilacji, więc każdy build daje inną wartość. Służy do
//  rozpoznania „wgrałem stare firmware", nie do kontroli zgodności —
//  od tego jest protoVersion.
// ---------------------------------------------------------------------
constexpr uint32_t protoFnv1a(const char* s, uint32_t h = 2166136261u) {
    return (*s == '\0') ? h : protoFnv1a(s + 1, (h ^ (uint32_t)(uint8_t)*s) * 16777619u);
}
constexpr uint32_t FW_BUILD_ID = protoFnv1a(__DATE__ __TIME__);
