<b>Opis Zadania</b>
Projekt symuluje działanie statku pasażerskiego, który kursuje między dwoma portami przy ograniczonym moście oraz ograniczonej pojemności pokładu. Jest to rozbudowana wersja klasycznego problemu współbieżności, w której należy zapewnić poprawne zarządzanie dostępem do statku i mostka, a także obsługę sygnałów i kończenie rejsów.

W projekcie występują trzy główne procesy:

Kapitan Statku – zarządza załadunkiem, rejsami i wyładunkiem pasażerów.
Kapitan Portu – wysyła sygnały (przyspieszenie wypłynięcia lub zakończenie rejsów).
Generator Pasażerów (plus procesy pasażerów) – tworzy pasażerów w losowych odstępach, a ci próbują wejść na statek, odbyć rejs, a następnie wylądować.
Warunki Początkowe
Statek ma pojemność N (liczba pasażerów, którzy mogą być na pokładzie naraz).
Mostek łączący ląd ze statkiem ma pojemność K (K < N) i może pomieścić maksymalnie K osób w tym samym czasie.
Liczba Rejsów do wykonania: R.
Czas jednego rejsu: T2.
Projekt zakłada:

Pasażerowie są tworzeni w pewnych odstępach czasu (przez proces Generatora).
Kapitan musi pilnować, by w momencie wypłynięcia mostek był pusty i liczba pasażerów nie przekraczała N.
Rejs może być przyspieszony sygnałem (sygnał1 – SIGUSR1) albo zakończony przed osiągnięciem R (sygnał2 – SIGUSR2).
Zasada Działania
Kapitan Statku
Oczekuje na załadunek pasażerów, dopóki statek się nie wypełni (lub nie nadejdzie sygnał1).
Jeśli statek gotowy, czeka aż mostek jest pusty, ogłasza wypłynięcie (rejs trwa T2), po czym następuje wyładunek.
Pilnuje też, czy nie przyszło polecenie zakończenia rejsów (sygnał2) – wtedy wywołuje forceUnload, ustawiając endOfDay.
Pasażer (oraz Generator Pasażerów)
Generator tworzy procesy pasażerów co pewien czas (np. co 1 sek).
Pasażer próbuje wejść na mostek (jeśli K miejsc jest zajętych, czeka), a następnie na statek (do N osób).
Czeka na rejs. Po zakończeniu rejsu czeka na wyładunek i opuszcza statek.
Może zrezygnować, jeśli statek już pełny lub nadchodzi koniec dnia (endOfDay).
Kapitan Portu
W pętli losowo (z określonym prawdopodobieństwem) wysyła sygnały do Kapitana Statku.
SIGUSR1 (sygnał1) – wymusza wcześniejsze wypłynięcie, jeśli trwa załadunek.
SIGUSR2 (sygnał2) – kończy rejsy: jeśli podczas załadunku – odwołuje rejs, jeśli w trakcie rejsu – kończy go i nie pozwala zacząć nowego.
Dodatkowe Polecenia Kierownika (Sygnały)
SIGUSR1: Kapitan Portu (lub inny proces) może wymusić wcześniejsze wypłynięcie statku (np. gdy nie chcemy czekać, aż statek osiągnie maks N).
SIGUSR2: powoduje zakończenie cyklu rejsów. Jeśli statek jeszcze się ładuje – forceUnload i koniec, jeśli statek w rejsie – zakończyć rejs i nie rozpoczynać kolejnego.
Kompilacja i Uruchamianie
Budowanie projektu:

bash
Kopiuj
Edytuj
make
Tworzy trzy programy:

kapitanStatku
pasazer (generator pasażerów)
kapitanPortu
Uruchamianie projektu
Korzystamy z reguły run w Makefile, przekazując parametry:

bash
Kopiuj
Edytuj
make run N=5 K=3 R=3 T2=5
Gdzie:

N – pojemnosc statku,
K – pojemnosc mostka,
R – maks liczba rejsow,
T2 – czas trwania jednego rejsu (sekundy).
Po kolei odpalane są:

KapitanStatku w tle (z zadanymi parametrami),
Generator pasażerów (w tle),
KapitanPortu (na pierwszym planie).
Zamykanie

Jeśli KapitanPortu dojdzie do wniosku, że endOfDay lub osiągnięto R rejsów, kończy działanie.
Pasażerowie kończą się, gdy endOfDay=true.
Kapitan Statku (po zakończeniu R rejsów lub sygnale2) sprząta zasoby IPC.
Pliki
common.h, common.cpp – obsługa semaforów, pamięci dzielonej, kolory w konsoli.
kapitanStatku.cpp – główna logika rejsów, sygnalizuje endOfDay, usuwa zasoby IPC.
pasazer.cpp – generator pasażerów (main) i funkcja onePassenger, określająca, jak pasażer wsiada i opuszcza statek.
kapitanPortu.cpp – wysyła sygnały w pętli, by wpływać na czas rejsów.
Komendy Make
make – kompiluje projekt.
make run N=... K=... R=... T2=... – uruchamia cały projekt z zadanymi parametrami.
make clean – usuwa pliki obiektowe i pliki binarne.
