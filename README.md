# Napęd łańcuchowy do okien
Oprogramowanie napędu łańcuchowego automatycznego domu.

### Budowa napędu łańcuchowego
Mechanizm napędu łańcuchowego został zbudowany na bazie ESP8266 wraz z modułem RTC DS1307. Uzupełnieniem jest silnik krokowy ze sterownikiem A4988.

### Pliki obudowy, części oraz dokładny opis montażu
Oryginalne pliki projektu Window Chain Actuator: https://www.thingiverse.com/thing:3577666

Pliki zmodyfikowane przeze mnie: https://www.thingiverse.com/thing:4580200

### Możliwości
Łączność z napędem łańcuchowym odbywa się przez sieć Wi-Fi.
Dane dostępowe do routera przechowywane są wraz z innymi informacjami w pamięci flash.
W przypadku braku informacji o sieci, urządzenie aktywuje wyszukiwania routera z wykorzystaniem funkcji WPS.

Napęd łańcuchowy automatycznie łączy się z zaprogramowaną siecią Wi-Fi w przypadku utraty połączenia.

Urządzenie posiada opcję wykonania pomiaru długości łańcucha oraz możliwość kalibracji. Funkcja pomiaru długości łańcucha wyklucza stosowanie ograniczników krańcowych.

Zegar czasu rzeczywistego wykorzystywany jest przez funkcję ustawień automatycznych.
Ustawienia automatyczne obejmują otwieranie, uchylanie i zamykanie okna o wybranej godzinie.
Powtarzalność obejmuje okres jednego tygodnia, a ustawienia nie są ograniczone ilościowo. W celu zminimalizowania objętości wykorzystany został zapis tożsamy ze zmienną boolean, czyli dopiero wystąpienie znaku wskazuje na włączoną funkcję.

* 'w' cały tydzień
* 'o' poniedziałek, 'u' wtorek, 'e' środa, 'h' czwartek, 'r' piątek, 'a' sobota, 's' niedziela
* '_' otwórz o godzinie - jeśli znak występuje w zapisie, przed nim znajduje się godzina w zapisie czasu uniksowego
* 'p' uchyl o godzinie - jeśli znak występuje w zapisie, przed nim znajduje się godzina w zapisie czasu uniksowego
* '-' zamknij o godzinie - jeśli występuje w zapisie, po nim znajduje się godzina w zapisie czasu uniksowego
* '/' wyłącz ustawienie - obecność znaku wskazuje, że ustawienie będzie ignorowane

Przykład zapisu ustawienia automatycznego: 1320pcw-420

Obecność znaku 'c' wskazuje, że ustawienie dotyczy napędu łańcuchowego.

### Sterowanie
Sterowanie urządzeniem odbywa się poprzez wykorzystanie metod dostępnych w protokole HTTP. Sterować można z przeglądarki lub dedykowanej aplikacji.

* "/hello" - Handshake wykorzystywany przez dedykowaną aplikację, służy do potwierdzenia tożsamości oraz przesłaniu wszystkich parametrów pracy urządzenia.

* "/set" - Pod ten adres przesyłane są ustawienia dla napędu łańcuchowego, dane przesyłane w formacie JSON. Ustawić można m.in. strefę czasową ("offset"), czas RTC ("time"), ustawienia automatyczne ("smart"), pozycję łańcucha ("val"), dokonać kalibracji łańcucha, jak również zmienić ilość kroków czy procentową wartość uchylenia okna.

* "/state" - Służy do regularnego odpytywania urządzenia o jego podstawowe stany, położenie łańcucha.

* "/reset" - Ustawia wartość pozycji łańcucha na 0.

* "/measurement" - Służy do wykonania pomiaru długości łańcucha.

* "/basicdata" - Służy innym urządzeniom systemu iDom do samokontroli, urządzenia po uruchomieniu odpytują się wzajemnie o aktualny czas lub dane z czujników.

* "/log" - Pod tym adresem znajduje się dziennik aktywności urządzenia (domyślnie wyłączony).

* "/wifisettings" - Ten adres służy do usunięcia danych dostępowych do routera.
