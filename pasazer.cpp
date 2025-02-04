#include "common.h"
#include <atomic>
#include <cstdlib>
#include <ctime>

static std::atomic<int> nextPassengerID{0}; // liczy kolejne ID pasazerow

// Funkcja dla pojedynczego pasazera (wywolywana w child po fork)
void onePassenger(int pId);

int main()
{
    colorLog("[PasazerGenerator] uruchomiony.", COL_BGREEN);

    srand(time(NULL));

    // Tworzenie klucza do pamieci dzielonej
    key_t keyShm = ftok(FTOK_PATH, 0x12);
    if(keyShm == -1) { perror("ftok pasazer shm"); exit(1); }
    // Proba pobrania segmentu pamieci (bez IPC_CREAT - zakladamy, ze juz jest)
    int shmid = shmget(keyShm, sizeof(SharedData), 0600);
    if(shmid == -1) {
        perror("pasazer shmget");
        exit(1);
    }
    // Dolaczamy sie do pamieci
    SharedData* shdata = attachShm(shmid);

    // Ustawiamy generatorPID w pamieci, by KapitanStatku wiedzial, kto jest generatorem
    shdata->generatorPID = getpid();

    // Tworzenie klucza do semaforow
    key_t keySem = ftok(FTOK_PATH, 0x11);
    if(keySem == -1) { perror("ftok pasazer sem"); exit(1); }
    int semid = semget(keySem, SEM_COUNT, 0600);
    if(semid == -1) {
        perror("pasazer semget");
        exit(1);
    }

    // Petla: generujemy pasazerow co 1 sek, dopoki endOfDay==false
    while(true) {
        if(shdata->endOfDay) {
            colorLog("[PasazerGenerator] przerwanie petli generowania, bo endOfDay.", COL_RED);
            break;
        }
        int pId = ++nextPassengerID;
        pid_t c = fork();
        if(c == 0) {
            // child => uruchamiamy onePassenger
            onePassenger(pId);
        } else if(c < 0) {
            perror("fork");
        }
        // Poczekaj 1 sek miedzy kolejnymi pasazerami
        usleep(1000000);
    }

    // Gdy petla sie konczy (endOfDay=true), czekamy blokujaco na wszystkie dzieci (pasazerow)
    while(true) {
        pid_t w = waitpid(-1, nullptr, 0);
        if(w<0) {
            if(errno==ECHILD) {
                break;
            }
            break;
        }
    }

    colorLog("[PasazerGenerator] zakonczyl prace (wszyscy pasazerowie zakonczeni).", COL_BRED);
    detachShm(shdata);
    return 0;
}

// Funkcja wywolana w procesie potomnym - jeden pasazer
void onePassenger(int pId)
{
    // Dolaczenie do pamieci
    key_t keyShm = ftok(FTOK_PATH, 0x12);
    int shmid = shmget(keyShm, sizeof(SharedData), 0600);
    if(shmid == -1) {
        perror("onePassenger shmget");
        exit(1);
    }
    SharedData* shdata = attachShm(shmid);

    // Dolaczenie do semaforow
    key_t keySem = ftok(FTOK_PATH, 0x11);
    int semid = semget(keySem, SEM_COUNT, 0600);
    if(semid == -1) {
        perror("onePassenger semget");
        exit(1);
    }

    // 1) Czekamy, az (SEM_DIR==1, loading==true, traveling==false) - faza zaladunku
    while(true) {
        if(shdata->endOfDay) {
            // jesli endOfDay==true, to pasazer rezygnuje
            std::string msg = "[Pasazer " + std::to_string(pId) + "] rezygnuje.";
            colorLog(msg, COL_RED);
            detachShm(shdata);
            exit(0);
        }
        int dirVal = getSemValue(semid, SEM_DIR);
        if(dirVal==1 && shdata->loading && !shdata->traveling) {
            break;
        }
        usleep(50000);
    }

    // 2) Proba wejscia na mostek
    {
        int valBridge = getSemValue(semid, SEM_BRIDGE);
        if(valBridge==0) {
            std::string msg = "[Pasazer " + std::to_string(pId) + "] czeka, bo mostek jest pelen.";
            colorLog(msg, COL_YELLOW);
        }
    }
    semOp(semid, SEM_BRIDGE, -1);

    // Jesli w trakcie zajmowania mostka loading sie wylaczy, traveling sie wlaczy lub endOfDay,
    // to pasazer rezygnuje
    if(!shdata->loading || shdata->traveling || shdata->endOfDay) {
        std::string msg = "[Pasazer " + std::to_string(pId) + "] rezygnuje.";
        colorLog(msg, COL_RED);
        semOp(semid, SEM_BRIDGE, +1);
        detachShm(shdata);
        exit(0);
    }

    {
        std::string msg = "[Pasazer " + std::to_string(pId) + "] wchodzi na mostek.";
        colorLog(msg, COL_YELLOW);
    }

    // 3) Sprawdzamy, czy statek jest juz pelny
    if(getSemValue(semid, SEM_SHIP)==0) {
        // jesli tak, to rezygnuje
        std::string msg = "[Pasazer " + std::to_string(pId) + "] rezygnuje.";
        colorLog(msg, COL_RED);
        semOp(semid, SEM_BRIDGE, +1);
        detachShm(shdata);
        exit(0);
    }

    // 4) Wchodzimy na statek
    semOp(semid, SEM_SHIP, -1);
    {
        std::string msg = "[Pasazer " + std::to_string(pId) + "] wchodzi na statek.";
        colorLog(msg, COL_GREEN);
    }
    semOp(semid, SEM_BRIDGE, +1);

    // 5) Czekamy na rejs (traveling=true->false)
    while(!shdata->traveling && !shdata->endOfDay) {
        usleep(50000);
    }
    while(shdata->traveling && !shdata->endOfDay) {
        usleep(50000);
    }

    // Jesli endOfDay w trakcie, to pasazer moze byc wywalony forceUnload
    // (ew. rezygnuje)
    // 6) Czekamy na wyladunek (SEM_DIR=0, disembarking=true)
    while(true) {
        int dVal = getSemValue(semid, SEM_DIR);
        if(dVal==0 && shdata->disembarking) {
            break;
        }
        if(shdata->endOfDay) {
            // moze forceUnload
        }
        usleep(50000);
    }

    // 7) Schodzenie
    {
        int valBridge = getSemValue(semid, SEM_BRIDGE);
        if(valBridge==0) {
            std::string msg = "[Pasazer " + std::to_string(pId) + "] czeka, bo mostek jest pelen (schodzenie).";
            colorLog(msg, COL_MAGENTA);
        }
    }
    semOp(semid, SEM_BRIDGE, -1);

    {
        std::string msg = "[Pasazer " + std::to_string(pId) + "] schodzi ze statku na mostek.";
        colorLog(msg, COL_MAGENTA);
    }

    semOp(semid, SEM_SHIP, +1);

    {
        std::string msg = "[Pasazer " + std::to_string(pId) + "] zszedl z mostka.";
        colorLog(msg, COL_MAGENTA);
    }
    semOp(semid, SEM_BRIDGE, +1);

    detachShm(shdata);
    exit(0);
}
