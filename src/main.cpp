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
static volatile uint32_t platHelloCount   = 0;
static volatile uint32_t lastPlatHelloMs  = 0;  // millis() ostatniego HELLO

// ---- Ostatnia odebrana telemetria ----
// Zapisywana w callbacku (task WiFi), czytana przez task wyświetlacza.
static SemaphoreHandle_t telemetryMutex = nullptr;
static Msg_Telemetry     lastTelemetry;
static volatile uint32_t lastTelemetryMs = 0;
static volatile uint32_t telemSeqLast    = 0;
static volatile uint32_t telemRecvCount  = 0;
static volatile uint32_t telemMissCount  = 0;

// Telemetria idzie 20 Hz, więc cisza dłuższa niż to = osiem zgubionych ramek.
// Zastępuje wykrywanie po HELLO, które przy interwale 5 s było bezużytecznie
// wolne — zostaje ono tylko dla przypadku „platformy nie widzieliśmy nigdy".
constexpr uint32_t TELEM_TIMEOUT_MS = 400;
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
static volatile uint32_t telemetryCount   = 0;
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
        platHelloCount++;
        lastPlatHelloMs  = millis();
        if (platProtoOk && handshakeAtMs == 0) handshakeAtMs = millis();
        return;
    }

    case MSG_TELEMETRY:
        if (len != (int)sizeof(Msg_Telemetry)) break;
        if (telemetryMutex && xSemaphoreTake(telemetryMutex, 0) == pdTRUE) {
            memcpy(&lastTelemetry, incomingData, sizeof(Msg_Telemetry));
            lastTelemetryMs = millis();
            telemetryCount++;
            // Luki w numeracji = ramki telemetrii zgubione po drodze.
            if (telemRecvCount > 0 && lastTelemetry.seq > telemSeqLast + 1) {
                telemMissCount += lastTelemetry.seq - telemSeqLast - 1;
            }
            telemSeqLast = lastTelemetry.seq;
            telemRecvCount++;
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
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (memcmp(mac_addr, macMonitorDebug, 6) == 0) {
        ESP_NOW_Monitor_Error = (status != ESP_NOW_SEND_SUCCESS);
        if (ESP_NOW_Monitor_Error) {
            ESP_NOW_Monitor_Send_Error_Counter++;
        }
    }

    if (memcmp(mac_addr, macPlatformMecanum, 6) == 0) {
        ESP_NOW_Platform_Error = (status != ESP_NOW_SEND_SUCCESS);
        if (ESP_NOW_Platform_Error) {
            ESP_NOW_Platform_Send_Error_Counter++;
        }
    }
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
    const TickType_t xFrequency = pdMS_TO_TICKS(50); // 20 Hz
    TickType_t xLastWakeTime = xTaskGetTickCount();
    Msg_PadControl localMsg;

    while (1) {
        // Copy shared data (protected by mutex)
        xSemaphoreTake(messageMutex, portMAX_DELAY);
        totalMessages++;
        message.seq = totalMessages;
        memcpy(&localMsg, &message, sizeof(Msg_PadControl));
        xSemaphoreGive(messageMutex);

        // Send to debug monitor (adresat do usunięcia w kroku sprzątania)
        esp_now_send(macMonitorDebug, (uint8_t *)&localMsg, sizeof(Msg_PadControl));

        // Send to mecanum platform
        esp_now_send(macPlatformMecanum, (uint8_t *)&localMsg, sizeof(Msg_PadControl));

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}


// --- TASK 3: Updating the TFT display (TaskTFTScreen) ---
// Every 50 ms, reads joystick and button states and updates the display accordingly.
void TaskTFTScreen(void *pvParameters) {
    (void)pvParameters;
    const TickType_t xFrequency = pdMS_TO_TICKS(50); // 20 Hz
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // Read current values from shared message (protected)
        xSemaphoreTake(messageMutex, portMAX_DELAY);
        int lx = message.axisLX;
        int ly = message.axisLY;
        int rx = message.axisRX;
        int ry = message.axisRY;

        // Bity są już znormalizowane w packButtons(): 1 = wciśnięty.
        uint16_t btn = message.buttons;
        xSemaphoreGive(messageMutex);

        bool L_Button_A      = btn & BTN_L_A;
        bool L_Button_B      = btn & BTN_L_B;
        bool L_Button_X      = btn & BTN_L_X;
        bool L_Button_Y      = btn & BTN_L_Y;
        bool L_Button_SELECT = btn & BTN_L_SELECT;
        bool L_Button_START  = btn & BTN_L_START;

        bool R_Button_A      = btn & BTN_R_A;
        bool R_Button_B      = btn & BTN_R_B;
        bool R_Button_X      = btn & BTN_R_X;
        bool R_Button_Y      = btn & BTN_R_Y;
        bool R_Button_SELECT = btn & BTN_R_SELECT;
        bool R_Button_START  = btn & BTN_R_START;

        // Echo osi z ostatniej telemetrii. Świeżość liczymy od odbioru ramki,
        // nie od jej timestampu — zegary obu urządzeń nie są zsynchronizowane.
        int  elx = 0, ely = 0, erx = 0, ery = 0;
        bool echoValid = false;
        if (xSemaphoreTake(telemetryMutex, portMAX_DELAY) == pdTRUE) {
            echoValid = (telemRecvCount > 0) &&
                        ((millis() - lastTelemetryMs) < TELEM_TIMEOUT_MS);
            elx = lastTelemetry.echoAxisLX;
            ely = lastTelemetry.echoAxisLY;
            erx = lastTelemetry.echoAxisRX;
            ery = lastTelemetry.echoAxisRY;
            xSemaphoreGive(telemetryMutex);
        }

        // Pas stanu w wolnym miejscu nad przyciskami. Ostrzeżenia mają
        // pierwszeństwo; komunikat o zgodnej wersji gaśnie po LINK_BANNER_MS
        // i zostawia pas pusty na to, co przyjdzie później.
        char linkText[24];
        uint16_t linkColor = TFT_GREEN;
        uint32_t nowMs = millis();

        if (protoErrorCount > 0) {
            snprintf(linkText, sizeof(linkText), "?TYP %u  LEN %d",
                     (unsigned)lastUnknownType, lastUnknownLen);
            linkColor = TFT_RED;
        } else if (!platSeen) {
            snprintf(linkText, sizeof(linkText), "PLAT --  szukam");
            linkColor = TFT_YELLOW;
        } else if (!platProtoOk) {
            snprintf(linkText, sizeof(linkText), "PLAT v%u  ZLA WERSJA",
                     (unsigned)platProtoVersion);
            linkColor = TFT_RED;
        } else if (telemRecvCount > 0 && !echoValid) {
            // Telemetria kiedyś dochodziła i przestała — to wykrycie jest
            // szybkie (20 Hz), więc ma pierwszeństwo przed ciszą HELLO.
            snprintf(linkText, sizeof(linkText), "PLAT ZGUBIONA");
            linkColor = TFT_RED;
        } else if (telemRecvCount == 0 &&
                   (nowMs - lastPlatHelloMs) > PLAT_HELLO_TIMEOUT_MS) {
            snprintf(linkText, sizeof(linkText), "PLAT ZGUBIONA");
            linkColor = TFT_RED;
        } else if ((nowMs - handshakeAtMs) < LINK_BANNER_MS) {
            snprintf(linkText, sizeof(linkText), "PLAT v%u  OK",
                     (unsigned)platProtoVersion);
            linkColor = TFT_GREEN;
        } else {
            linkText[0] = '\0';   // pas pusty — miejsce zwolnione
        }

        // Przerysowujemy tylko przy zmianie. Bez tego pas szedłby po SPI
        // 20 razy na sekundę bez powodu, konkurując z resztą ekranu.
        static char     lastText[24] = { 1 };
        static uint16_t lastColor    = 0;
        if (strcmp(linkText, lastText) != 0 || linkColor != lastColor) {
            display.updateLinkStatus(linkText, linkColor);
            strncpy(lastText, linkText, sizeof(lastText));
            lastColor = linkColor;
        }

        // Update display with latest joystick positions and button states
        display.updateJoystick(lx, ly, rx, ry, echoValid, elx, ely, erx, ery);
        display.updateButtonsL(L_Button_A, L_Button_B, L_Button_X, L_Button_Y, L_Button_SELECT, L_Button_START);
        display.updateButtonsR(R_Button_A, R_Button_B, R_Button_X, R_Button_Y, R_Button_SELECT, R_Button_START);
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

    // Add debug monitor peer
    memcpy(peerInfo.peer_addr, macMonitorDebug, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer (monitor)");
        return;
    }

    // Add mecanum platform peer
    memcpy(peerInfo.peer_addr, macPlatformMecanum, 6);
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