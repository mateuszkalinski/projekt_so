#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cerrno>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>

/*
   Plik naglowkowy zawiera stale, struktury i funkcje wspolne dla wszystkich plikow .cpp.
   Zawiera tez deklaracje funkcji obslugi semaforow i pamieci dzielonej.
*/

// --------------------- KONFIGURACJA ----------------------

// Pojemnosc statku
static const int N = 5;
// Pojemnosc mostka
static const int K = 3;
// Maksymalna liczba rejsow
static const int R = 3;

// Czas na zaladunek (lub oczekiwanie do rejsu) w sekundach
static const int T1 = 10;
// Czas trwania rejsu
static const int T2 = 5;

/*
   Sygnaly:
   - SIGUSR1 => przyspieszenie rejsu (jesli statek jest w trakcie zaladunku).
   - SIGUSR2 => zakonczenie rejsow w danym dniu.
      Jesli w trakcie zaladunku => rejs sie nie odbedzie, pasazerowie schodza.
      Jesli w trakcie rejsu => statek dokonczy i konczy dzien.
*/

// Klucze do IPC
static const char* FTOK_PATH = "/tmp"; 
static const int FTOK_ID_SEM = 65;
static const int FTOK_ID_SHM = 66;

// Indeksy semaforow w tablicy
enum {
    SEM_BRIDGE = 0,  // semafor ograniczajacy liczbe osob na moscie
    SEM_SHIP   = 1,  // semafor ograniczajacy liczbe osob na statku
    SEM_DIR    = 2,  // semafor pomocniczy do blokowania kierunku (zaladunek/wyladunek)
    SEM_COUNT  = 3
};

// Struktura przechowywana w pamieci dzielonej
struct SharedData {
    pid_t kapitanStatkuPID;   // PID kapitana statku

    bool endOfDay;     // czy otrzymano sygnal2 (zakonczenie dzialalnosci)
    bool traveling;    // czy statek jest w rejsie
    bool loading;      // czy trwa zaladunek
    bool disembarking; // czy trwa wyadunek pasazerow

    int rejsCount;     // ile rejsow dotad wykonano
};

// Do semctl
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Deklaracje funkcji do semaforow
int  createOrGetSemaphore(key_t key);
int  setSemValue(int semid, int semnum, int value);
int  getSemValue(int semid, int semnum);
void semOp(int semid, int semnum, int op);

// Deklaracje funkcji do pamieci dzielonej
int         createOrGetShm(key_t key);
SharedData* attachShm(int shmid);
void        detachShm(const void* addr);
void        removeShm(int shmid);

// Funkcja pomocnicza do logow
inline void logMsg(const std::string &msg) {
    std::cout << "[" << getpid() << "] " << msg << std::endl;
}

#endif
//test