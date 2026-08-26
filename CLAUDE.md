# CLAUDE.md — Pad Controller (Xiao ESP32-S3)

Pilot do platformy mecanum: dwa joysticki Adafruit Seesaw (I2C), wyświetlacz
ST7735 1.8", FreeRTOS, ESP-NOW. Ten plik zawiera wyłącznie rzeczy, których nie
widać z samego kodu: trwałe niezmienniki i decyzje architektoniczne. Świadomie
**nie** ma tu statusu prac ani planów — to rotuje najszybciej.

## Architektura systemu

System składa się z **dwóch** urządzeń: tego Pada i platformy mecanum
(repo `ESP32S3_Mecanum_Base`, katalog lokalny
`~/Documents/PlatformIO/Projects/Platforma_czterokolowa_esp32` — nazwa katalogu
nie odpowiada nazwie repo).

Osobny moduł „debug monitor" i aplikacja na iPhone zostały **porzucone**
(decyzja z 2026-08-23), żeby szybciej dowieźć działającą całość. **Pad przejmuje
ich rolę** — ma własny wyświetlacz i jest w rękach operatora, więc docelowo
pokazuje nie tylko własny stan, ale też telemetrię odebraną z platformy.

Konsekwencje dla tego repo:

- Pad odbiera już `MSG_HELLO` i **liczy** przychodzące ramki telemetrii, ale
  jeszcze ich nie wyświetla — od tego jest `DisplayManager` i osobny krok.
- `macMonitorDebug`, wysyłka do monitora w `TaskESPNow` oraz liczniki
  `ESP_NOW_Monitor_*` w `errors.h` są przeznaczone do usunięcia, nie do rozwijania.

## Niezmiennik: `src/messages.h` musi być identyczny z kopią w repo platformy

Plik definiuje protokół ESP-NOW i **oba repo muszą mieć go bajt w bajt takiego
samego**. Każda zmiana struktury to zmiana w obu repo w tym samym kroku.

Typ wiadomości rozpoznaje **pierwszy bajt** (`msgType`), długość służy tylko do
walidacji przed `memcpy`. Wcześniej rozpoznawano po samym `sizeof`, co przy
przypadkowej zgodności rozmiarów dawało ciche czytanie danych jako niewłaściwej
struktury. `static_assert` na rozmiarach zamienia przypadkową edycję w jednym
repo w błąd kompilacji.

Wersję protokołu niosą `MSG_HELLO` nadawane okresowo przez obie strony — nie
leci ona w każdej ramce. Platforma **nie ruszy**, dopóki nie zobaczy od Pada
HELLO ze zgodną wersją, więc rozjazd wersji objawia się staniem w miejscu
i czerwonym napisem na wyświetlaczu, a nie zgadywaniem.

## Nazewnictwo adresów MAC

Szablon `src/mac_addresses.h` w tym repo nazywa adres Pada `macModulXiao`,
a platforma w swoim kodzie oczekuje `macPadXiao`. To ten sam fizyczny układ pod
dwiema nazwami — warto zgrać przy okazji porządkowania protokołu.

## MAC-y i budowanie

Prawdziwe adresy leżą w `src/mac_addresses_private.h`, który jest w `.gitignore`
i dołączany warunkowo przez `__has_include`; w razie braku używany jest szablon
`src/mac_addresses.h` z wyzerowanymi adresami. Nie commituj prywatnej kopii.

Konfiguracja TFT_eSPI (sterownik, piny, prędkość SPI, fonty) siedzi w
`build_flags` w `platformio.ini` — **nie** edytuj `User_Setup.h` w bibliotece.
Ostrzeżenie o `TOUCH_CS` podczas budowania jest oczekiwane: użyty panel ST7735
nie ma warstwy dotykowej.

Budowanie: `pio run` (środowisko `seeed_xiao_esp32s3`). Każdy push i pull request
jest sprawdzany przez GitHub Actions (`.github/workflows/build.yml`).

## Jak weryfikować zmiany

Kompilacja i zielone CI mówią tylko tyle, że kod jest poprawny składniowo.
Zmiany w odczycie joysticków, protokole i wyświetlaniu weryfikuje się wgraniem
na sprzęt. Przyjęty rytm pracy: jedna zamknięta zmiana → build → flash →
sprawdzenie → dopiero następna zmiana.
