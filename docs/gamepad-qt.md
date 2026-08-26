# Adafruit Mini I2C STEMMA QT Gamepad (PID 5743)

Ściągawka sprzętowa dla dwóch gamepadów użytych w Padzie. Spisana z
dokumentacji Adafruita 2026-08-26, żeby nie zgadywać i nie szukać za każdym
razem. Źródła na końcu.

## Numeracja pinów seesaw

Kontroler seesaw (ATtiny817) wystawia własne numery pinów — **nie** są to piny
ESP32 i nie są ciągłe:

| funkcja | pin seesaw |
|---|---|
| przycisk A | 5 |
| przycisk B | 1 |
| przycisk X | 6 |
| przycisk Y | 2 |
| SELECT | 0 |
| START | 16 |
| joystick oś X | 14 (`analogRead`) |
| joystick oś Y | 15 (`analogRead`) |

Sześć przycisków na sztukę, więc przy dwóch gamepadach **dwanaście** stanów do
przesłania. Numery są rzadkie (0,1,2,5,6,16) — najwyższy bit to 16, dlatego kod
trzyma je w `uint32_t`. Do wysyłki w eter warto je przepakować gęsto:
dwanaście bitów mieści się w `uint16_t` z czterema wolnymi.

## Poziomy i zakresy

- Przyciski są **aktywne stanem niskim**: konfiguracja `INPUT_PULLUP`, wciśnięty
  = bit 0. Stąd negacja w kodzie: `!(buttons & (1UL << BUTTON_A))`.
- Joystick: ADC **10-bitowy**, zakres `0..1023`. Przykład Adafruita odwraca oś
  (`1023 - analogRead(14)`), żeby zgadzała się z orientacją drążka.
- Zasilanie: tym samym napięciem, co logika mikrokontrolera — tutaj 3,3 V.

## Adresy I2C

Domyślnie `0x50`. Zworki adresowe (przecięcie ścieżki):

| zworki | adres |
|---|---|
| brak (fabrycznie) | 0x50 |
| przecięta A0 | 0x51 |
| przecięta A1 | 0x52 |
| przecięte A0 i A1 | 0x53 |

W Padzie: lewy `GAMEPAD1_ADDR = 0x50`, prawy `GAMEPAD2_ADDR = 0x51` (czyli
w prawym przecięta A0). Magistrala: `Wire.begin(5, 6)` — SDA 5, SCL 6.

## Czego nasz kod nie robi, a przykład Adafruita robi

Przykład sprawdza po `ss.getVersion()`, czy pod adresem siedzi układ o product
ID **5743**. Nasz `setup()` sprawdza tylko, czy `begin()` się powiodło — a to
przejdzie także dla innego układu seesaw pod tym samym adresem. Tanie
zabezpieczenie przed pomyleniem płytek.

## Źródła

- https://learn.adafruit.com/gamepad-qt/pinouts
- https://learn.adafruit.com/gamepad-qt/arduino
- https://github.com/adafruit/Adafruit_Seesaw/blob/master/examples/Mini_I2C_Gamepad_QT/Mini_I2C_Gamepad_QT.ino
