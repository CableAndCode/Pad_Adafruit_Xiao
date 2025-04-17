## Road Map – Pad Controller (ESP-NOW, Xiao ESP32-S3)

### Etap 1 – Minimalna funkcjonalność (MVP)
- [x] Równoczesne wykonywanie zadań (FreeRTOS)
- [x] Odczyt surowych wartości z joysticków
- [x] Obsługa martwej strefy, histerezy i skalowania
- [x] Serializacja danych do struktury
- [x] Wysyłanie danych przez ESP-NOW do platformy
- [x] Odbiór potwierdzony na platformie
- [x] Wizualizacja danych na wyświetlaczu TFT
- [x] Mechanizmy synchronizacji (ochrony spójności danych): użycie mutexa 

### Etap 2 – Diagnostyka i interfejs lokalny
- [ ] Usunięcie wizualizacji przycisków, lub ich przebudowa o jak najmniejszego rozmiaru
- [ ] Dodanie pomiaru siły sygnału RSSI (funkcja esp_now_get_peer_rssi()), wyświetlenie na TFT
- [ ] Ekran początkowy z wyświetleniem informacji po starcie układu, status połączeń
- [ ] Wyświetlanie statusu błędów Pada na TFT
- [ ] Wyświetlanie statusu błędów Platformy Mecanum na TFT
- [ ] Wyśweitlanie statusu błędów Monitora na TFT (gdy już Monitor będzie w stanie wysyłać te błędy i statusy)
- [ ] Wyświetlanie Parametrów Platformy Mecanum


### Etap 3 – Zasilanie i ergonomia
- [ ] Migracja z breadboard na płytkę uniwersalną, dodanie fizycznego włącznika - lutowanie
- [ ] Integracja z baterią - ładowanie, pomiar, wyświetlanie na TFT
- [ ] Obsługa uśpienia i wybudzania np.: czas, przyciski
- [ ] Dodanie dodatkowego/dodatkowych Ekranu/ów z inną wizualizacją parametrów
- [ ] Obsługa przycisków (L_select, L_start, L_mode LXY LAB, RXY RAB, R_select, R_start, R_mode)
- [ ] Obudowa robocza lub docelowa


### Etap 4 – Rozszerzenia opcjonalne
- [ ] Szyfrowanie ESP-NOW
- [ ] Konfigurowalne profile sterowania

