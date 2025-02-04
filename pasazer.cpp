#include "common.h"
#include <atomic>
#include <cstdlib>
#include <ctime>

static std::atomic<int> nextPassengerID{0};

void onePassenger(int pId);

int main()
{
    colorLog("[PasazerGenerator] uruchomiony.", COL_BGREEN);

    srand(time(NULL));

    key_t keyShm = ftok(FTOK_PATH, 0x12);
    if(keyShm == -1) { perror("ftok pasazer shm"); exit(1); }
    int shmid = shmget(keyShm, sizeof(SharedData), 0600);
    if(shmid == -1) {
        perror("pasazer shmget");
        exit(1);
    }
    SharedData* shdata = attachShm(shmid);

    // Ustawiamy generatorPID
    shdata->generatorPID = getpid();

    key_t keySem = ftok(FTOK_PATH, 0x11);
    if(keySem == -1) { perror("ftok pasazer sem"); exit(1); }
    int semid = semget(keySem, SEM_COUNT, 0600);
    if(semid == -1) {
        perror("pasazer semget");
        exit(1);
    }

    // Generujemy pasazerow co 1 sek, dopoki endOfDay==false
    while(true) {
        if(shdata->endOfDay) {
            colorLog("[PasazerGenerator] przerwanie petli generowania, bo endOfDay.", COL_RED);
            break;
        }
        int pId = ++nextPassengerID;
        pid_t c = fork();
        if(c == 0) {
            // child => onePassenger
            onePassenger(pId);
        } else if(c < 0) {
            perror("fork");
        }
        usleep(30000000); // 1 sek
    }

    // Gdy endOfDay, musimy jeszcze poczekac na wszystkie dzieci
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

void onePassenger(int pId)
{
    key_t keyShm = ftok(FTOK_PATH, 0x12);
    int shmid = shmget(keyShm, sizeof(SharedData), 0600);
    if(shmid == -1) {
        perror("onePassenger shmget");
        exit(1);
    }
    SharedData* shdata = attachShm(shmid);

    key_t keySem = ftok(FTOK_PATH, 0x11);
    int semid = semget(keySem, SEM_COUNT, 0600);
    if(semid == -1) {
        perror("onePassenger semget");
        exit(1);
    }

    // Petla czekania => SEM_DIR=1 i loading=true i traveling=false
    while(true) {
        if(shdata->endOfDay) {
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

    // czy mostek pelen
    {
        int valBridge = getSemValue(semid, SEM_BRIDGE);
        if(valBridge==0) {
            std::string msg = "[Pasazer " + std::to_string(pId) + "] czeka, bo mostek jest pelen.";
            colorLog(msg, COL_YELLOW);
        }
    }
    semOp(semid, SEM_BRIDGE, -1);

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

    // ewentualnie usleep(500000);

    // czy statek pelny
    if(getSemValue(semid, SEM_SHIP)==0) {
        std::string msg = "[Pasazer " + std::to_string(pId) + "] rezygnuje.";
        colorLog(msg, COL_RED);
        semOp(semid, SEM_BRIDGE, +1);
        detachShm(shdata);
        exit(0);
    }

    // wchodzimy na statek
    semOp(semid, SEM_SHIP, -1);
    {
        std::string msg = "[Pasazer " + std::to_string(pId) + "] wchodzi na statek.";
        colorLog(msg, COL_GREEN);
    }
    semOp(semid, SEM_BRIDGE, +1);

    // czekamy rejs
    while(!shdata->traveling && !shdata->endOfDay) {
        usleep(50000);
    }
    while(shdata->traveling && !shdata->endOfDay) {
        usleep(50000);
    }
    if(shdata->endOfDay) {
        // moze forceUnload itp
    }

    // wyladunek => SEM_DIR=0, disembarking=true
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

    // schodzimy
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