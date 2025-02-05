# Projekt "Rejs" – Temat 1

## Opis Zadania

Symulacja działania **statku pasażerskiego**, który kursuje między portami, mając ograniczoną przepustowość **mostka** (K) i pokładu **statku** (N).  
- Statek może wykonać maksymalnie **R** rejsów w ciągu dnia.  
- Każdy rejs trwa **T2** sekund.  
- Pasażerowie przychodzą losowo i próbują wejść na statek, o ile dostępne są miejsca na moście (K) i pokładzie (N).

W projekcie występują **3 główne procesy**:
1. **Kapitan Statku** – zarządza załadunkiem pasażerów, wypłynięciem i wyładunkiem po rejsie.  
2. **Generator Pasażerów** (i sami pasażerowie) – tworzy procesy pasażerów w losowych odstępach czasu; każdy pasażer próbuje wejść na pokład i odbyć rejs.  
3. **Kapitan Portu** – wysyła sygnały do Kapitana Statku (wcześniejsze wypłynięcie, koniec rejsów).

## Warunki Początkowe

- **N** – pojemność statku (liczba pasażerów, którzy mogą jednocześnie przebywać na pokładzie).  
- **K** – pojemność mostka (maksymalna liczba osób wchodzących/wychodzących jednocześnie).  
- **R** – maksymalna liczba rejsów do wykonania w danym dniu.  
- **T2** – czas trwania jednego rejsu (w sekundach).

Dodatkowo:
- Statek nie może wypłynąć, dopóki na moście są ludzie.  
- Jeśli sygnał **SIGUSR2** nadejdzie podczas załadunku, Kapitan Statku przerwie rejs i wyprosi pasażerów (forceUnload).  
- Jeśli sygnał nadejdzie podczas rejsu, statek dokończy obecny rejs i nie rozpocznie kolejnego.

## Struktura Programu

Pliki źródłowe:

- **`common.h` / `common.cpp`**  
  Zawierają funkcje pomocnicze do obsługi pamięci współdzielonej i semaforów (System V), a także definicje kolorowego logowania w konsoli.

- **`kapitanStatku.cpp`**  
  Inicjuje zasoby (sem, shm), uruchamia główną pętlę rejsów (załadunek -> wypłynięcie -> wyładunek), reaguje na sygnały (SIGUSR1, SIGUSR2) i ostatecznie usuwa zasoby IPC.

- **`pasazer.cpp`**  
  Uruchamia **Generator Pasażerów**, który w pętli tworzy (fork) procesy pasażerów. Każdy pasażer stara się wejść na mostek, pokład statku, czeka na rejs i potem schodzi. Jeśli statek jest już pełny albo **endOfDay** nadejdzie, pasażerowie rezygnują.

- **`kapitanPortu.cpp`**  
  Losowo, co kilka sekund, wysyła sygnał **SIGUSR1** (wymuszone wypłynięcie) bądź **SIGUSR2** (koniec rejsów) do Kapitana Statku. Kończy się, gdy `rejsCount >= R` lub `endOfDay`.

## Sygnały

- **SIGUSR1**  
  Kapitan Statku otrzymuje go i skraca fazę załadunku (wcześniejsze wypłynięcie).  
- **SIGUSR2**  
  Kapitan Statku kończy rejsy:  
  - Jeśli podczas załadunku: forceUnload i koniec dnia,  
  - Jeśli podczas rejsu: kończy bieżący rejs i nie zaczyna następnego.

## Kompilacja i Uruchamianie

1. **Kompilacja**  
   W katalogu z plikami wywołaj:
   ```bash
   make
