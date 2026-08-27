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
#include "errors.h"
#include "joystick_read.h"
#include "DisplayManager.h"


// --- Hardware Configuration ---

DisplayManager display; // TFT display manager
Adafruit_seesaw ss1, ss2; // I2C-based gamepad controllers

JoystickReader joystickReaderL(offsetL_X, offsetL_Y, true, true);
JoystickReader joystickReaderR(offsetR_X, offsetR_Y, false, false);

Msg_PadControl message; // Global message structure from pad

// ---- Stan protokołu (patrz nagłówek messages.h) ----
static volatile uint8_t  platProtoVersion = 0;      // wersja ogłoszona przez platformę
static volatile bool     platSeen         = false;  // widziano jakiekolwiek HELLO
static volatile bool     platProtoOk      = false;  // ...i wersja się zgadza
static volatile uint32_t lastPlatHelloMs  = 0;  // millis() ostatniego HELLO

// ---- Ostatnia odebrana telemetria ----
// Zapisywana w callbacku (task WiFi), czytana przez task wyświetlacza.
static SemaphoreHandle_t telemetryMutex = nullptr;
static Msg_Telemetry     lastTelemetry;
static volatile uint32_t lastTelemetryMs = 0;
static volatile uint32_t telemSeqLast    = 0;
static volatile bool     telemEverSeen   = false;  // znacznik trwały
static volatile uint32_t telemRecvCount  = 0;      // licznik okna, zerowany
static volatile uint32_t telemMissCount  = 0;

// Telemetria idzie 20 Hz, więc cisza dłuższa niż to = osiem zgubionych ramek.
// Zastępuje wykrywanie po HELLO, które przy interwale 5 s było bezużytecznie
// wolne — zostaje ono tylko dla przypadku „platformy nie widzieliśmy nigdy".
constexpr uint32_t TELEM_TIMEOUT_MS = 400;

// Czas wysyłki ramki o danym numerze — do policzenia drogi w obie strony
// z echoSeq. Przy 50 Hz 64 pozycje to ~1,3 s historii, grubo powyżej
// jakiegokolwiek sensownego opóźnienia.
constexpr uint32_t RTT_RING = 64;
static volatile uint32_t sendMs[RTT_RING] = { 0 };
static volatile uint32_t lastRttMs = 0;
static volatile uint32_t handshakeAtMs    = 0;  // moment pierwszej zgodnej wersji

// Napis z wersją jest KOMUNIKATEM STARTOWYM, nie stałym elementem — po
// kilku sekundach znika i zwalnia pas na siłę sygnału oraz ostrzeżenia.
// Informacja „wersje się zgadzają" jest potrzebna raz; to, co ma tam być
// na stałe, zmienia się w trakcie jazdy.
constexpr uint32_t LINK_BANNER_MS = 4000;

// Cisza dłuższa niż to = platforma zgubiona. Próg jest wysoki, bo dziś
// jedynym sygnałem od platformy są HELLO co 5 s. Gdy telemetria zacznie
// docierać do Pada (20 Hz), wykrywanie zejdzie do ułamka sekundy.
constexpr uint32_t PLAT_HELLO_TIMEOUT_MS = 15000;
static volatile uint32_t protoErrorCount  = 0;      // ramki nieznanego typu/długości
static volatile uint8_t  lastUnknownType  = 0;
static volatile int      lastUnknownLen   = 0;

// Przepakowanie przycisków z rzadkiej numeracji pinów seesaw (0,1,2,5,6,16)
// na gęste bity protokołu. Przy okazji normalizacja: seesaw daje stan NISKI
// przy wciśnięciu, w eter idzie 1 = WCIŚNIĘTY, żeby odbiorca nie musiał
// wiedzieć nic o pull-upach.
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

// Callback odbioru — Pad po raz pierwszy CZEGOKOLWIEK słucha. Rozpoznanie po
// pierwszym bajcie, długość jako walidacja (patrz nagłówek messages.h).
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len < 1) return;
    const uint8_t type = incomingData[0];

    switch (type) {
    case MSG_HELLO: {
        if (len != (int)sizeof(Msg_Hello)) break;
        Msg_Hello hello;
        memcpy(&hello, incomingData, sizeof(hello));
        if (hello.role != ROLE_PLATFORM) break;
        platProtoVersion = hello.protoVersion;
        platSeen         = true;
        platProtoOk      = (hello.protoVersion == PROTO_VERSION);
        lastPlatHelloMs  = millis();
        if (platProtoOk && handshakeAtMs == 0) handshakeAtMs = millis();
        return;
    }

    case MSG_TELEMETRY:
        if (len != (int)sizeof(Msg_Telemetry)) break;
        if (telemetryMutex && xSemaphoreTake(telemetryMutex, 0) == pdTRUE) {
            memcpy(&lastTelemetry, incomingData, sizeof(Msg_Telemetry));
            lastTelemetryMs = millis();
            // Luki w numeracji = ramki telemetrii zgubione po drodze.
            if (telemEverSeen && lastTelemetry.seq > telemSeqLast + 1) {
                telemMissCount += lastTelemetry.seq - telemSeqLast - 1;
            }
            telemSeqLast = lastTelemetry.seq;
            telemRecvCount++;
            telemEverSeen = true;

            // Droga w obie strony: od wysłania ramki o numerze echoSeq do
            // chwili, gdy wróciła w telemetrii. Mierzone na JEDNYM zegarze,
            // więc rozjazd zegarów obu urządzeń nie ma tu znaczenia.
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

// Ogłaszanie wersji protokołu — powtarzane, bo ESP-NOW nie zna sesji
// i platforma może się zresetować w dowolnej chwili.
void TaskHello(void *pvParameters) {
    (void)pvParameters;
    while (1) {
        Msg_Hello hello = {};
        hello.msgType      = MSG_HELLO;
        hello.protoVersion = PROTO_VERSION;
        hello.role         = ROLE_PAD;
        hello.fwBuildId    = FW_BUILD_ID;
        hello.uptimeMs     = millis();
        esp_now_send(macPlatformMecanum, (uint8_t *)&hello, sizeof(hello));
        vTaskDelay(pdMS_TO_TICKS(platProtoOk ? 5000 : 1000));
    }
}

// --- Callback: Confirm delivery status of sent ESP-NOW messages ---
// Okno ostatnich ACK_WINDOW wysyłek. Suma od uruchomienia tylko rośnie i nic
// nie mówi — liczy się udział strat TERAZ, bo to on odzwierciedla zasięg.
constexpr int ACK_WINDOW = 100;
static volatile uint8_t  ackFail[ACK_WINDOW] = { 0 };
static volatile int      ackIndex = 0;
static volatile int      ackFailCount = 0;      // ile jedynek jest w oknie
static volatile int      ackStreak = 0;         // ile strat POD RZĄD

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (memcmp(mac_addr, macPlatformMecanum, 6) != 0) return;

    const bool failed = (status != ESP_NOW_SEND_SUCCESS);
    ESP_NOW_Platform_Error = failed;
    if (failed) ESP_NOW_Platform_Send_Error_Counter++;

    ackFailCount -= ackFail[ackIndex];
    ackFail[ackIndex] = failed ? 1 : 0;
    ackFailCount += ackFail[ackIndex];
    ackIndex = (ackIndex + 1) % ACK_WINDOW;

    ackStreak = failed ? (ackStreak + 1) : 0;
}

// Udział strat przełożony na „zasięg". Odwzorowanie jest CELOWO nieliniowe:
// przy krawędzi zasięgu straty wystrzeliwują, więc kilka procent zgubionych
// pakietów oznacza, że zapasu prawie nie ma. Punkty: 0% strat = 100 zasięgu,
// 5% = 50, 10% = 20, 20% i więcej = 0.
static unsigned rangeFromLoss(unsigned lossPercent) {
    if (lossPercent == 0)  return 100;
    if (lossPercent <= 5)  return 100 - (lossPercent * 50) / 5;
    if (lossPercent <= 10) return 50  - ((lossPercent - 5) * 30) / 5;
    if (lossPercent <= 20) return 20  - ((lossPercent - 10) * 20) / 10;
    return 0;
}




// --- TASK 1: Reading gamepad input (TaskGamepads) ---
// Every 20 ms, reads and normalizes joystick values and updates the global message structure.
// Mutex-protected access ensures safe sharing of data.
void TaskGamepads(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // 50 Hz
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        unsigned long localTimeStamp = millis();

        // Read raw joystick values
        int localL_Joystick_raw_x = ss1.analogRead(14);
        int localL_Joystick_raw_y = ss1.analogRead(15);
        int localR_Joystick_raw_x = ss2.analogRead(14);
        int localR_Joystick_raw_y = ss2.analogRead(15);

        // Normalize joystick values using offset correction
        int localL_Joystick_x = joystickReaderL.getCorrectedValueX(localL_Joystick_raw_x);
        int localL_Joystick_y = joystickReaderL.getCorrectedValueY(localL_Joystick_raw_y);
        int localR_Joystick_x = joystickReaderR.getCorrectedValueX(localR_Joystick_raw_x);
        int localR_Joystick_y = joystickReaderR.getCorrectedValueY(localR_Joystick_raw_y);

        // Read button states
        int localL_Joystick_buttons_message = ss1.digitalReadBulk(button_mask);
        int localR_Joystick_buttons_message = ss2.digitalReadBulk(button_mask2);

        // Critical section: update global message structure.
        // Wartości surowe nie trafiają już w eter — kalibracja odbywa się tutaj,
        // a platforma nigdy ich nie czytała.
        xSemaphoreTake(messageMutex, portMAX_DELAY);
        message.msgType   = MSG_PAD_CONTROL;
        message.mode      = 0;
        message.timeStamp = localTimeStamp;

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

// --- TASK 2: Sending data via ESP-NOW (TaskESPNow) ---
// Every 50 ms, copies the latest gamepad data and transmits it via ESP-NOW to monitor and platform receivers.
void TaskESPNow(void *pvParameters) {
    (void)pvParameters;
    // 50 Hz, zgodnie z tempem odczytu drążków. Przy 20 Hz trzy niezależne
    // pętle 50 ms (wysyłka, telemetria, rysowanie) przesuwały się fazami
    // względem siebie i odstępy między aktualizacjami kropki skakały od 0
    // do 100 ms — to, a nie sama częstotliwość, wyglądało jak szarpanie.
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // 50 Hz
    TickType_t xLastWakeTime = xTaskGetTickCount();
    Msg_PadControl localMsg;

    while (1) {
        // Copy shared data (protected by mutex)
        xSemaphoreTake(messageMutex, portMAX_DELAY);
        totalMessages++;
        message.seq = totalMessages;
        sendMs[totalMessages % RTT_RING] = millis();
        memcpy(&localMsg, &message, sizeof(Msg_PadControl));
        xSemaphoreGive(messageMutex);

        // Jedyny odbiorca. Wysyłka pod adres porzuconego monitora kosztowała
        // ponawianie transmisji przez warstwę MAC (unicast bez ACK) tuż przed
        // ramką, która ma znaczenie — i to było widać jako drgająca kropka.
        esp_now_send(macPlatformMecanum, (uint8_t *)&localMsg, sizeof(Msg_PadControl));

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}


// Ekran powitalny ustępuje po handshake, a gdyby platformy nie było wcale —
// najpóźniej po tym czasie, żeby Pad nie zawiesił się na nim w nieskończoność.
constexpr uint32_t SPLASH_MAX_MS = 6000;

// Strata ramek telemetrii liczona w oknie, nie od startu — inaczej po kilku
// minutach jazdy pojedyncze zgubione ramki rozpuszczałyby się w średniej.
static unsigned telemLossPermille = 0;

// Pas stanu: ostrzeżenia mają pierwszeństwo, a gdy wszystko gra, pokazuje to,
// co zmienia się w trakcie jazdy — drogę w obie strony i stratę ramek.
static void drawLinkStatusLine(uint32_t rttMs, int y) {
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

    if (protoErrorCount > 0) {
        snprintf(text, sizeof(text), "?TYP %u  LEN %d",
                 (unsigned)lastUnknownType, lastUnknownLen);
        color = TFT_RED;
    } else if (!platSeen) {
        snprintf(text, sizeof(text), "PLAT --  szukam");
        color = TFT_YELLOW;
    } else if (!platProtoOk) {
        snprintf(text, sizeof(text), "PLAT v%u  ZLA WERSJA",
                 (unsigned)platProtoVersion);
        color = TFT_RED;
    } else if (telemEverSeen && !echoFresh) {
        snprintf(text, sizeof(text), "PLAT ZGUBIONA");
        color = TFT_RED;
    } else if (!telemEverSeen &&
               (nowMs - lastPlatHelloMs) > PLAT_HELLO_TIMEOUT_MS) {
        snprintf(text, sizeof(text), "PLAT ZGUBIONA");
        color = TFT_RED;
    } else if ((nowMs - handshakeAtMs) < LINK_BANNER_MS) {
        snprintf(text, sizeof(text), "PLAT v%u  OK", (unsigned)platProtoVersion);
        color = TFT_GREEN;
    } else {
        snprintf(text, sizeof(text), "RTT %ums  L%u",
                 (unsigned)rttMs, telemLossPermille);
        color = TFT_CYAN;
    }

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

// --- Ekrany ---
// Przełączanie jest LOKALNE na Padzie: wybór widoku nie dotyczy platformy,
// więc nie ma po co zajmować nim ani jednego bitu w eterze.
enum Screen : uint8_t {
    SCR_DRIVE = 0,   // wektory prędkości — zadany i rzeczywisty
    SCR_WHEELS,      // cztery koła osobno
    SCR_LINK,        // jakość łącza w liczbach
    SCR_BUTTONS,     // test przycisków
    SCR_COUNT
};

// SELECT lewego pada przewija ekrany w przód, prawego w tył.
// START zostaje WOLNY — jest zarezerwowany pod przyszły przycisk awaryjny
// i nie może kolidować z nawigacją.
static uint8_t currentScreen = SCR_DRIVE;

// --- TASK 3: Updating the TFT display (TaskTFTScreen) ---
// Co 20 ms odczytuje stan drążków, przycisków i telemetrii, po czym rysuje
// aktywny ekran. Każdy panel sam pilnuje, czy jego dane się zmieniły.
void TaskTFTScreen(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // 50 Hz
    TickType_t xLastWakeTime = xTaskGetTickCount();

    bool splashActive = true;
    bool prevSelL = false, prevSelR = false;
    uint32_t bootMs = millis();

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // Read current values from shared message (protected)
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

        // Kopia telemetrii pod mutexem — dalej pracujemy już na swojej kopii,
        // żeby nie trzymać blokady przez czas rysowania.
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

        // --- ekran powitalny: ustępuje po handshake albo po SPLASH_MAX_MS ---
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
                if (!platSeen)          snprintf(st, sizeof(st), "szukam platformy...");
                else if (!platProtoOk)  snprintf(st, sizeof(st), "zla wersja: v%u",
                                                 (unsigned)platProtoVersion);
                else                    snprintf(st, sizeof(st), "polaczono");
                if (strcmp(st, splashStatus) != 0) {
                    display.showSplash(PROTO_VERSION, FW_BUILD_ID, st);
                    strncpy(splashStatus, st, sizeof(splashStatus));
                }
                continue;
            }
        }

        // --- nawigacja: zbocze narastające, żeby przytrzymanie nie przewijało ---
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

        // Ekran jazdy ma własny układ: pas stanu na górze, radar na całą resztę.
        // Pierścienie drążków są tam zbędne — radar dowodzi tego samego i więcej.
        const bool radarLayout = (currentScreen == SCR_DRIVE);
        drawLinkStatusLine(rtt, radarLayout ? 0 : 65);
        if (!radarLayout) {
            display.updateJoystick(lx, ly, rx, ry, echoValid,
                                   tel.echoAxisLX, tel.echoAxisLY,
                                   tel.echoAxisRX, tel.echoAxisRY);
        }

        switch (currentScreen) {
        case SCR_DRIVE: {
            // Wektory odtworzone z obrotów czterech kół.
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
                              telemLossPermille, protoErrorCount);
            break;
        }
        case SCR_BUTTONS:
            display.panelButtons(L, R);
            break;
        }
    }
}

// --- Setup function ---
// Initializes all hardware and software components before entering task loop.
void setup() {
    Serial.begin(115200);
    Wire.begin(5, 6); // I2C setup: SDA = 5, SCL = 6
    display.begin();

    // Initialize gamepads
    if (!ss1.begin(GAMEPAD1_ADDR) || !ss2.begin(GAMEPAD2_ADDR)) {
        Serial.println("Gamepad not found!");
        while (1) delay(100);
    }
    Serial.println("Gamepad OK!");

    // Configure input pins for buttons
    ss1.pinModeBulk(button_mask, INPUT_PULLUP);
    ss1.setGPIOInterrupts(button_mask, 1);
    ss2.pinModeBulk(button_mask2, INPUT_PULLUP);
    ss2.setGPIOInterrupts(button_mask2, 1);
#ifdef IRQ_PIN
    pinMode(IRQ_PIN, INPUT);
#endif

    // Read initial joystick offsets
    offsetL_X = ss1.analogRead(14);
    offsetL_Y = ss1.analogRead(15);
    offsetR_X = ss2.analogRead(14);
    offsetR_Y = ss2.analogRead(15);
    joystickReaderL.setOffset(offsetL_X, offsetL_Y);
    joystickReaderR.setOffset(offsetR_X, offsetR_Y);

    // Mutexy przed ESP-NOW — patrz komentarz przy register_recv_cb.
    messageMutex   = xSemaphoreCreateMutex();
    telemetryMutex = xSemaphoreCreateMutex();
    if (messageMutex == NULL || telemetryMutex == NULL) {
        Serial.println("Failed to create mutex!");
        while (1) delay(100);
    }

    // Initialize ESP-NOW communication
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Failed");
        return;
    }
    esp_now_register_send_cb(OnDataSent);
    // Rejestracja callbacku odbioru MUSI nastąpić po utworzeniu muteksów:
    // task WiFi może wywołać OnDataRecv natychmiast, gdy setup() jeszcze trwa.
    esp_now_register_recv_cb(OnDataRecv);

    // Add mecanum platform peer — jedyny partner Pada
    memcpy(peerInfo.peer_addr, macPlatformMecanum, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer (platform)");
        return;
    }

    // Create FreeRTOS tasks
    xTaskCreate(TaskGamepads, "Gamepads", 2048, NULL, 1, NULL);
    xTaskCreate(TaskESPNow, "ESPNowSend", 2048, NULL, 1, NULL);
    xTaskCreate(TaskTFTScreen, "TFTScreen", 4096, NULL, 1, NULL);
    xTaskCreate(TaskHello,     "Hello",      2048, NULL, 1, NULL);
}

// --- Main loop ---
// Empty loop, all logic is handled in FreeRTOS tasks.
void loop() {
    // Nothing to do here
}