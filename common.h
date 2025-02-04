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

// Definicja sciezki dla ftok (tworzenie kluczy IPC)
static const char* FTOK_PATH = "/tmp";

// --------------------- Kolory ANSI ---------------------
// Te definicje pozwalaja kolorowac tekst w konsoli (np. logi).
// COL_RESET zawsze wraca do normalnego koloru.
#define COL_RESET   "\033[0m"
#define COL_BOLD    "\033[1m"

#define COL_RED     "\033[31m"
#define COL_GREEN   "\033[32m"
#define COL_YELLOW  "\033[33m"
#define COL_BLUE    "\033[34m"
#define COL_MAGENTA "\033[35m"
#define COL_CYAN    "\033[36m"
#define COL_WHITE   "\033[37m"
#define COL_GRAY    "\033[90m"

#define COL_BRED    "\033[1;31m"
#define COL_BGREEN  "\033[1;32m"
#define COL_BBLUE   "\033[1;34m"
#define COL_BCYAN   "\033[1;36m"
#define COL_BWHITE  "\033[1;37m"

// Funkcja do logowania w kolorze w konsoli
inline void colorLog(const std::string &msg, const char* color)
{
    std::cout << color << msg << COL_RESET << std::endl;
}

// --------------------- SEMAFORY, PAMIEC DZIELONA ---------------------

// Indeksy semaforow:
// SEM_BRIDGE - ogranicza liczbe osob na moscie,
// SEM_SHIP - ogranicza liczbe osob na statku,
// SEM_DIR - okresla kierunek (1 = zaladunek, 0 = wyladunek).
enum {
    SEM_BRIDGE = 0,
    SEM_SHIP   = 1,
    SEM_DIR    = 2,
    SEM_COUNT  = 3
};

// Struktura przechowywana w pamieci dzielonej.
// Zawiera:
//  - PID Kapitana Statku i Generatora Pasaerow
//  - flage endOfDay informujaca o koncu dnia
//  - informacje o stanie zaladunku, rejsu, itp.
//  - parametry N, K, R, T2
struct SharedData {
    pid_t kapitanStatkuPID;
    pid_t generatorPID;

    bool endOfDay;       // gdy true, oznacza zakonczenie dnia
    bool traveling;      // czy statek jest w rejsie
    bool loading;        // czy trwa zaladunek
    bool disembarking;   // czy trwa wyladunek
    int  rejsCount;      // ile rejsow wykonano

    int N;  // pojemnosc statku
    int K;  // pojemnosc mostka
    int R;  // ile rejsow max
    int T2; // czas jednego rejsu
};

// Struktura uzywana do operacji semctl
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Deklaracje funkcji do obslugi semaforow i pamieci dzielonej

int  createOrGetSemaphore(key_t key);
int  setSemValue(int semid, int semnum, int value);
int  getSemValue(int semid, int semnum);
void semOp(int semid, int semnum, int op);

int  createOrGetShm(key_t key);
SharedData* attachShm(int shmid);
void detachShm(const void* addr);
void removeShm(int shmid);

#endif
