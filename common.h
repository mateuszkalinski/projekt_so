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
   Konfiguracja:
   N - pojemnosc statku
   K - pojemnosc mostka
   R - maksymalna liczba rejsow
   T2 - czas rejsu
*/

static const int N = 5;  
static const int K = 3;  
static const int R = 3;  
static const int T2 = 5;  

// sciezka i ID do ftok
static const char* FTOK_PATH = "/tmp";
static const int FTOK_ID_SEM = 0x70;
static const int FTOK_ID_SHM = 0x71;

/*
   Semafory:
   SEM_BRIDGE -> ogranicza liczbe osob na moscie (K)
   SEM_SHIP   -> ogranicza liczbe osob na statku (N)
   SEM_DIR    -> 1=zaladunek, 0=wyadunek
*/
enum {
    SEM_BRIDGE = 0,
    SEM_SHIP   = 1,
    SEM_DIR    = 2,
    SEM_COUNT  = 3
};

struct SharedData {
    pid_t kapitanStatkuPID;

    bool endOfDay;       // sygnal2 => koniec rejsow
    bool traveling;      // czy statek jest w rejsie
    bool loading;        // czy trwa zaladunek
    bool disembarking;   // czy trwa wyadunek
    int  rejsCount;      // ile rejsow wykonano
};

// Struktura do semctl
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Deklaracje semaforow
int  createOrGetSemaphore(key_t key);
int  setSemValue(int semid, int semnum, int value);
int  getSemValue(int semid, int semnum);
void semOp(int semid, int semnum, int op);

// Deklaracje pamieci dzielonej
int         createOrGetShm(key_t key);
SharedData* attachShm(int shmid);
void        detachShm(const void* addr);
void        removeShm(int shmid);

// Funkcja pomocnicza do logow
inline void logMsg(const std::string &msg)
{
    std::cout << "[" << getpid() << "] " << msg << std::endl;
}

#endif
