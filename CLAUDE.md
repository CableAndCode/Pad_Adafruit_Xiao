# CLAUDE.md — Pad Controller (Xiao ESP32-S3)

> Working notes for this repository, kept in Polish by choice. Everything a
> user or contributor needs is in README.md and docs/, in English.

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

- Pad odbiera `MSG_HELLO` i telemetrię z platformy. Z telemetrii rysuje na
  razie **kropkę echa** w pierścieniach drążków; reszta pól czeka na
  zagospodarowanie.
- Ślady po monitorze zostały usunięte w całości: wysyłka, peer, liczniki
  `ESP_NOW_Monitor_*` i adres w szablonie MAC. Platforma jest jedynym
  partnerem Pada.

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

## Pas stanu na wyświetlaczu (y = 65..88)

Ten pas jest wolny: ramki joysticków kończą się na y=63, sprite'y przycisków
zaczynają na y=90. Napis z wersją protokołu jest tam **komunikatem startowym**,
nie stałym elementem — gaśnie kilka sekund po handshake'u. Docelowo pas należy
do siły sygnału i ostrzeżeń, bo informacja „wersje się zgadzają" jest potrzebna
raz, a to, co się zmienia w trakcie jazdy, potrzebuje miejsca na stałe.

Ekran ma **dwa układy**. Zwykły: pierścienie drążków (y 0..63), pas stanu
(y 65..88) i panel dolny (y 90..159). Radarowy (ekran jazdy): pas stanu na górze
(y 0..23) i radar na całą resztę (y 24..159). Zmiana ekranu czyści cały ekran,
bo układy zajmują różne obszary.

Na radarze pierścienie drążków są **celowo pominięte**. Kropka echa dowodziła,
że platforma czyta właściwe pola; radar dowodzi więcej, bo rysuje się z obrotów
kół, czyli potwierdza także kinematykę i regulator.

Panele dolne dzielą jeden sprite, radar ma własny — osobny sprite na każdy ekran
kosztowałby po kilkanaście kB. `createSprite()` przy braku pamięci zwraca
`nullptr` i **cicho przestaje rysować**, dlatego `begin()` sprawdza wynik.

Wybór ekranu jest **lokalny dla Pada** — nie zajmuje ani jednego bitu w eterze,
bo platformy nie obchodzi, na co patrzy operator. SELECT lewego pada przewija
w przód, prawego w tył. **START jest celowo wolny** — zarezerwowany pod przyszły
przycisk awaryjny, który nie może kolidować z nawigacją.

Tempo: drążki 50 Hz, wysyłka 50 Hz, telemetria z platformy 25 Hz (odpowiedź
na co drugą ramkę), rysowanie 50 Hz. Te okresy muszą pozostać **zbliżone**. Gdy wysyłka, telemetria
i rysowanie chodziły po 20 Hz, trzy niezsynchronizowane okresy 50 ms dawały
odstępy między aktualizacjami kropki od 0 do 100 ms — kropka szarpała mimo
zdrowego łącza i zerowych strat. Spowolnienie któregokolwiek z tych zadań
przywróci ten objaw.

Docelowa forma sprzężenia zwrotnego z platformy jest graficzna, nie tekstowa:
kropka odesłanych osi w pierścieniu drążka, a w przyszłości wektor kierunku
i przyspieszenia — zadany z drążka i odesłany z platformy, obok siebie. Przy
mecanum ma to sens szczególny, bo jazda bokiem jest pełnoprawnym kierunkiem.

## Język: kod po angielsku, ten plik po polsku

Komentarze w `src/`, README i `docs/` są **po angielsku** — repo jest publiczne.
Ten plik zostaje po polsku, bo jest roboczy.

**Opisy commitów: od 2026-08-31 po angielsku**, tak samo jak w repo platformy.
Decyzja Piotra: polski opis pod angielskim README wygląda niespójnie, a listę
commitów widać na GitHubie tak samo jak kod. **Historii nie przepisujemy** —
starsze opisy zostają po polsku i nie są długiem do spłacenia.

**Napisy na wyświetlaczu też są po angielsku** — decyzja Piotra z 2026-08-29,
odwracająca poprzednią. Wcześniej stało tu, że zostają po polsku jako interfejs
operatora; skoro repo jest wizytówką, ekran na zdjęciu czyta ktoś z zewnątrz
tak samo jak kod. Skróty wolno stosować, byle po angielsku.

Ograniczenie, o którym trzeba pamiętać przy każdym nowym napisie: ekran ma
**128 pikseli**, a czcionka rozmiaru 1 zajmuje 6 pikseli na znak — czyli
**maksimum 21 znaków w wierszu**, mniej, jeśli kursor startuje z odsunięciem.
Rozmiar 2 to 12 pikseli na znak, więc 10 znaków. Angielski bywa dłuższy od
polskiego i to jest jedyny realny powód, dla którego napis może nie wejść;
wtedy skracaj, nie zmniejszaj czcionki. Trzy wiersze w `panelLink` trzymają
etykiety w kolumnie **9 znaków**, żeby liczby stały równo — nie psuj tego.

Nie ujednolicaj tego w żadną stronę bez pytania.

Ściągawka do gamepadów: [docs/gamepad-qt.md](docs/gamepad-qt.md).

## Kolejność wgrywania przy zmianie wersji protokołu

**Najpierw platforma, potem Pad, a Padowi na koniec odetnij zasilanie.**

Powód jest konkretny: ramka o niezgodnej długości trafia do `protoErrorCount`,
a ten licznik **nigdy się nie zeruje**. Jeśli wgrasz Pada jako pierwszego, przez
chwilę odbiera telemetrię starej wersji, licznik rośnie i zostaje taki do
restartu — panel łącza pokazuje błędy, których w tej chwili już nie ma.

Od poprawki B3 nie jest to groźne: `?TYPE` stoi w pasku statusu **poniżej**
stanów bezpieczeństwa, więc nie zasłania już `PLAT LOST`, `BAD VER` ani
`DRIVE CUT`. Ale licznik na panelu łącza dalej kłamie o teraźniejszości,
a restart Pada kosztuje sekundę.

## MAC-y i budowanie

Prawdziwe adresy leżą w `src/mac_addresses_private.h`, który jest w `.gitignore`
i dołączany warunkowo przez `__has_include`; w razie braku używany jest szablon
`src/mac_addresses.h` z wyzerowanymi adresami. Nie commituj prywatnej kopii.

Konfiguracja TFT_eSPI (sterownik, piny, prędkość SPI, fonty) siedzi w
`build_flags` w `platformio.ini` — **nie** edytuj `User_Setup.h` w bibliotece.
Ostrzeżenie o `TOUCH_CS` podczas budowania jest oczekiwane: użyty panel ST7735
nie ma warstwy dotykowej.

W `.gitignore` obowiązuje wzorzec `*private*`, a nie konkretna nazwa — konkretna
nazwa nie chroni przed literówką.

Wersja platformy jest **przypięta**: `espressif32 @ ~7.0.1`, identycznie jak
w repo platformy. Powód leży po tamtej stronie (core 3.x usunął API używane
przez `Motor.cpp`), ale wersje trzymamy zgodne, żeby oba urządzenia budowały
się tym samym łańcuchem.

`monitor_speed = 115200` jest w `platformio.ini` celowo: kod woła
`Serial.begin(115200)`, a bez tej linii `pio device monitor` staje na 9600
i pokazuje krzaki. Przy diagnostyce startu to różnica między odczytaniem
komunikatu a jego brakiem.

Budowanie: `pio run` (środowisko `seeed_xiao_esp32s3`). Każdy push i pull request
jest sprawdzany przez GitHub Actions (`.github/workflows/build.yml`).

## Biały ekran po wgraniu

Zdarzyło się 2026-08-28 i kosztowało kilka godzin szukania błędu w kodzie,
którego tam nie było. **Najpierw powtórz wgranie i zresetuj płytkę.**

Objawy potrafią złożyć się w bardzo przekonującą historię o awarii: ekran
biały (nie czarny, czyli panel bez inicjalizacji), port USB cyklicznie znika
i wraca, Serial milczy mimo poprawnego `ARDUINO_USB_CDC_ON_BOOT=1`, i to samo
dzieje się na **nowym** Xiao. Wszystko to były skutki jednej niedokończonej
transmisji. Dopiero gdy powtórzony flash nie pomoże — kabel USB i port, potem
kod.

## Jak weryfikować zmiany

Kompilacja i zielone CI mówią tylko tyle, że kod jest poprawny składniowo.
Zmiany w odczycie joysticków, protokole i wyświetlaniu weryfikuje się wgraniem
na sprzęt. Przyjęty rytm pracy: jedna zamknięta zmiana → build → flash →
sprawdzenie → dopiero następna zmiana.
