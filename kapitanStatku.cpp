#include "common.h"
#include <cstring>

static volatile sig_atomic_t earlyDeparture = 0; // flaga sygnalu1

static int semid = -1;
static int shmid = -1;
static SharedData* shdata = nullptr;

// Handler SIGUSR1 => wczesniejsze wyplyniecie
void sigusr1_handler(int)
{
    earlyDeparture = 1;
}

// Handler SIGUSR2 => endOfDay
void sigusr2_handler(int)
{
    if(shdata) {
        shdata->endOfDay = true;
    }
}

// Funkcja wymuszajaca rozladowanie (jesli sygnal2 w zaladunku)
void forceUnload()
{
    logMsg("forceUnload: wymuszam rozladowanie pasazerow.");
    shdata->loading = false;
    shdata->disembarking = true;
    setSemValue(semid, SEM_DIR, 0);

    // dopoki statek nie jest pusty
    while(true) {
        int freeOnShip = getSemValue(semid, SEM_SHIP);
        if(freeOnShip == N) {
            break;
        }
        semOp(semid, SEM_SHIP, +1);
        usleep(100000);
    }
    // dopoki mostek nie jest pusty
    while(true) {
        int freeOnBridge = getSemValue(semid, SEM_BRIDGE);
        if(freeOnBridge == K) {
            break;
        }
        semOp(semid, SEM_BRIDGE, +1);
        usleep(100000);
    }
    logMsg("forceUnload: rozladowanie zakonczone.");
}

int main()
{
    key_t keySem = ftok(FTOK_PATH, FTOK_ID_SEM);
    key_t keyShm = ftok(FTOK_PATH, FTOK_ID_SHM);

    semid = createOrGetSemaphore(keySem);
    shmid = createOrGetShm(keyShm);
    shdata = attachShm(shmid);

    // Inicjalizacja semaforow
    setSemValue(semid, SEM_BRIDGE, K);
    setSemValue(semid, SEM_SHIP,   N);
    setSemValue(semid, SEM_DIR,    1);

    // Stan
    shdata->kapitanStatkuPID = getpid();
    shdata->endOfDay = false;
    shdata->traveling = false;
    shdata->loading = true;
    shdata->disembarking = false;
    shdata->rejsCount = 0;

    logMsg("Kapitan Statku: start. (K="+std::to_string(K)+", N="+std::to_string(N)+")");

    // sygnaly
    struct sigaction sa1, sa2;
    memset(&sa1, 0, sizeof(sa1));
    sa1.sa_handler = sigusr1_handler;
    sigaction(SIGUSR1, &sa1, nullptr);

    memset(&sa2, 0, sizeof(sa2));
    sa2.sa_handler = sigusr2_handler;
    sigaction(SIGUSR2, &sa2, nullptr);

    while(true) {
        if(shdata->rejsCount >= R || shdata->endOfDay) {
            break;
        }

        // Zaladunek
        earlyDeparture = 0;
        shdata->loading = true;
        shdata->disembarking = false;
        setSemValue(semid, SEM_DIR, 1);

        logMsg("Zaladunek: czekam, az statek sie wypelni lub sygnal1. Jesli sygnal2 -> end.");

        while(true) {
            if(shdata->endOfDay) {
                logMsg("Sygnal2 w trakcie zaladunku => forceUnload i koniec.");
                forceUnload();
                goto KONIEC;
            }
            if(earlyDeparture) {
                logMsg("Sygnal1 => wczesniejsze wyplyniecie (koncze zaladunek).");
                break;
            }
            int freeOnShip = getSemValue(semid, SEM_SHIP);
            if(freeOnShip == 0) {
                logMsg("Statek jest pelny! Koncze zaladunek.");
                break;
            }
            usleep(200000);
        }

        shdata->loading = false;
        if(shdata->endOfDay) {
            break;
        }

        int passengersOnShip = N - getSemValue(semid, SEM_SHIP);
        logMsg("Koniec zaladunku. Na statku jest "+std::to_string(passengersOnShip)+" pasazerow.");

        // SLOW SLEEP(1) (opcjonalnie)
        sleep(1);

        // czekamy az mostek pusty
        while(true) {
            if(shdata->endOfDay) {
                logMsg("Sygnal2 w trakcie czekania na pusty mostek => forceUnload i koniec.");
                forceUnload();
                goto KONIEC;
            }
            int valBridge = getSemValue(semid, SEM_BRIDGE);
            if(valBridge == K) {
                break;
            }
            usleep(200000);
        }

        // Rejs
        shdata->traveling = true;
        logMsg("Wyplywam w rejs nr "+std::to_string(shdata->rejsCount+1)+", T2="+std::to_string(T2)+"s");
        sleep(T2);

        shdata->traveling = false;
        shdata->rejsCount++;
        logMsg("Wrocilem z rejsu nr "+std::to_string(shdata->rejsCount));

        if(shdata->endOfDay) {
            logMsg("Sygnal2 w trakcie rejsu => dokonczyc i rozladunek, koniec.");
            forceUnload();
            break;
        }

        // wyadunek
        shdata->disembarking = true;
        setSemValue(semid, SEM_DIR, 0);
        logMsg("Rozpoczynam wyadunek po rejsie...");

        while(true) {
            int freeOnShip = getSemValue(semid, SEM_SHIP);
            if(freeOnShip == N) {
                break;
            }
            if(shdata->endOfDay) {
                logMsg("Sygnal2 w trakcie wyadunku => forceUnload i koniec.");
                forceUnload();
                goto KONIEC;
            }
            usleep(200000);
        }
        shdata->disembarking = false;
        logMsg("Zakonczylem wyadunek.");

        if(shdata->rejsCount >= R) {
            logMsg("Osiagnieto R="+std::to_string(R)+". Koniec dnia.");
            shdata->endOfDay = true;
            break;
        }
    }

KONIEC:
    logMsg("Kapitan Statku konczy. rejsCount="+std::to_string(shdata->rejsCount)+
           " endOfDay="+std::to_string(shdata->endOfDay));

    detachShm(shdata);
    return 0;
}
