#include "common.h"
#include <atomic>
#include <cstdlib>
#include <ctime>

static std::atomic<int> nextPassengerID{0};

void onePassenger(int pId);

int main()
{
    logMsg("Proces PASAZER (generator) uruchomiony.");
    srand(time(NULL));

    key_t keyShm = ftok(FTOK_PATH, FTOK_ID_SHM);
    int shmid = shmget(keyShm, sizeof(SharedData), 0666);
    if(shmid == -1) {
        perror("pasazer shmget");
        exit(1);
    }
    SharedData* shdata = attachShm(shmid);

    key_t keySem = ftok(FTOK_PATH, FTOK_ID_SEM);
    int semid = semget(keySem, SEM_COUNT, 0666);
    if(semid == -1) {
        perror("pasazer semget");
        exit(1);
    }

    // W petli tworzymy pasazerow dopoki endOfDay == false
    while(true) {
        if(shdata->endOfDay) {
            logMsg("Generator pasazerow konczy, bo endOfDay.");
            break;
        }
        int pid = ++nextPassengerID;
        pid_t c = fork();
        if(c == 0) {
            onePassenger(pid);
        } else if(c < 0) {
            perror("fork");
        }

        // Minimalna pauza, zeby pojawialo sie kilka osob
        usleep(300000); // 0.3 sek
    }

    // czekamy na dzieci
    while(true) {
        pid_t w = waitpid(-1, nullptr, WNOHANG);
        if(w <= 0) break;
    }

    logMsg("Generator pasazerow zakonczyl prace.");
    detachShm(shdata);
    return 0;
}

void onePassenger(int pId)
{
    key_t keyShm = ftok(FTOK_PATH, FTOK_ID_SHM);
    int shmid = shmget(keyShm, sizeof(SharedData), 0666);
    if(shmid == -1) {
        perror("onePassenger shmget");
        exit(1);
    }
    SharedData* shdata = attachShm(shmid);

    key_t keySem = ftok(FTOK_PATH, FTOK_ID_SEM);
    int semid = semget(keySem, SEM_COUNT, 0666);
    if(semid == -1) {
        perror("onePassenger semget");
        exit(1);
    }

    // 1) czekamy na faze zaladunku => SEM_DIR=1, loading=true, traveling=false
    while(true) {
        if(shdata->endOfDay) {
            logMsg("Pasazer "+std::to_string(pId)+": rezygnuje, bo endOfDay (nie zdazyl wejsc).");
            detachShm(shdata);
            exit(0);
        }
        int dirVal = getSemValue(semid, SEM_DIR);
        if(dirVal == 1 && shdata->loading && !shdata->traveling) {
            break;
        }
        usleep(50000);
    }

    // Sprawdzamy czy mostek jest pelny -> jesli tak, log "czeka"
    // Ale w semop -1 i tak bedziemy czekac, jesli jest 0:
    {
        int valBridge = getSemValue(semid, SEM_BRIDGE);
        if(valBridge == 0) {
            logMsg("Pasazer "+std::to_string(pId)+" czeka, bo mostek jest pelen.");
        }
    }
    // Wchodzimy na mostek
    semOp(semid, SEM_BRIDGE, -1);

    // Jesli w miedzyczasie loading=false
    if(!shdata->loading || shdata->traveling || shdata->endOfDay) {
        logMsg("Pasazer "+std::to_string(pId)+" rezygnuje, bo loading=false lub traveling=true czy endOfDay.");
        semOp(semid, SEM_BRIDGE, +1);
        detachShm(shdata);
        exit(0);
    }

    logMsg("Pasazer "+std::to_string(pId)+" wchodzi na mostek.");

    // SLOW_MODE
    sleep(1);

    // Czy statek jest pelny -> rezygnujemy
    if(getSemValue(semid, SEM_SHIP) == 0) {
        logMsg("Pasazer "+std::to_string(pId)+" widzi, ze statek jest pelny. Rezygnuje.");
        semOp(semid, SEM_BRIDGE, +1);
        detachShm(shdata);
        exit(0);
    }
    
    // Rowniez mozna logowac "czeka, bo statek pelen" jesli SEM_SHIP==0, 
    // ale tu akurat rezygnuje, wiec moze nie czekac.

    // Wchodzimy na statek
    semOp(semid, SEM_SHIP, -1);
    logMsg("Pasazer "+std::to_string(pId)+" wchodzi na statek.");

    // Zwalniamy mostek
    semOp(semid, SEM_BRIDGE, +1);

    // SLOW_MODE
    sleep(1);

    // 2) czekamy na rejs (traveling=true->false)
    while(!shdata->traveling && !shdata->endOfDay) {
        usleep(50000);
    }
    while(shdata->traveling && !shdata->endOfDay) {
        usleep(50000);
    }

    // 3) czekamy na wyadunek => SEM_DIR=0, disembarking=true
    while(true) {
        if(shdata->endOfDay) {
            // moze nas w miedzyczasie usunac forceUnload
        }
        int dVal = getSemValue(semid, SEM_DIR);
        if(dVal == 0 && shdata->disembarking) {
            break;
        }
        usleep(50000);
    }

    // Schodzimy: mostek--
    {
        int valBridge = getSemValue(semid, SEM_BRIDGE);
        if(valBridge == 0) {
            logMsg("Pasazer "+std::to_string(pId)+" czeka, bo mostek jest pelen (schodzenie).");
        }
    }
    semOp(semid, SEM_BRIDGE, -1);

    logMsg("Pasazer "+std::to_string(pId)+" schodzi ze statku na mostek.");
    
    // SLOW_MODE
    sleep(1);

    // Zwolniamy statek
    semOp(semid, SEM_SHIP, +1);

    logMsg("Pasazer "+std::to_string(pId)+" zszedl z mostka.");
    semOp(semid, SEM_BRIDGE, +1);

    detachShm(shdata);
    exit(0);
}
