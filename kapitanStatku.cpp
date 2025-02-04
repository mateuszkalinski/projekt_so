#include "common.h"
#include <cstring>

// flaga sygnalu1 (wczesniejsze wyplyniecie)
static volatile sig_atomic_t earlyDeparture = 0;

static int semid = -1; // identyfikator semaforow
static int shmid = -1; // identyfikator pamieci dzielonej
static SharedData* shdata = nullptr;

// Sygnal1 => wczesniejsze wyplyniecie
void sigusr1_handler(int)
{
    earlyDeparture = 1;
}

// Sygnal2 => endOfDay
void sigusr2_handler(int)
{
    // Ustawiamy w pamieci dzielonej flage endOfDay
    if(shdata) {
        shdata->endOfDay = true;
    }
}

// forceUnload => wyladowanie pasazerow natychmiast
// ustawia SEM_DIR=0 i zwieksza semafory, by nikt nie zostal na statku/mostku
void forceUnload()
{
    colorLog("[KapitanStatku] forceUnload: wymuszam wyladowanie pasazerow!", COL_BRED);
    shdata->loading = false;
    shdata->disembarking = true;
    setSemValue(semid, SEM_DIR, 0);

    // Zwiekszamy SEM_SHIP dopoki nie bedzie N (pusty statek)
    while(true) {
        int freeOnShip = getSemValue(semid, SEM_SHIP);
        if(freeOnShip == shdata->N) {
            break;
        }
        semOp(semid, SEM_SHIP, +1);
        usleep(100000);
    }
    // Zwiekszamy SEM_BRIDGE dopoki nie bedzie K (pusty mostek)
    while(true) {
        int freeOnBridge = getSemValue(semid, SEM_BRIDGE);
        if(freeOnBridge == shdata->K) {
            break;
        }
        semOp(semid, SEM_BRIDGE, +1);
        usleep(100000);
    }
    colorLog("[KapitanStatku] forceUnload: wyladowanie zakonczone.", COL_BRED);
}

int main(int argc, char* argv[])
{
    // Odczyt argumentow: N, K, R, T2
    if(argc < 5) {
        std::cerr << "Uzycie: " << argv[0] << " <N> <K> <R> <T2>\n";
        exit(1);
    }
    int N_ = atoi(argv[1]);
    int K_ = atoi(argv[2]);
    int R_ = atoi(argv[3]);
    int T2_ = atoi(argv[4]);

    // Walidacja parametrow
    if(N_<=0 || K_<=0 || R_<=0 || T2_<=0) {
        std::cerr << "Parametry musza byc > 0!\n";
        exit(1);
    }
    if(K_ >= N_) {
        std::cerr << "K < N musi byc!\n";
        exit(1);
    }

    // Tworzenie kluczy IPC
    key_t keySem = ftok(FTOK_PATH, 0x11);
    if(keySem == -1) { perror("ftok sem"); exit(1); }
    key_t keyShm = ftok(FTOK_PATH, 0x12);
    if(keyShm == -1) { perror("ftok shm"); exit(1); }

    // Tworzymy semafory: SEM_COUNT = 3
    semid = semget(keySem, SEM_COUNT, 0600 | IPC_CREAT);
    if(semid == -1) {
        perror("semget");
        exit(1);
    }
    // Ustaw wartosci: mostek=K_, statek=N_, SEM_DIR=1 (zaladunek)
    setSemValue(semid, SEM_BRIDGE, K_);
    setSemValue(semid, SEM_SHIP,   N_);
    setSemValue(semid, SEM_DIR,    1);

    // Tworzymy pamiec dzielona
    shmid = shmget(keyShm, sizeof(SharedData), 0600 | IPC_CREAT);
    if(shmid == -1) {
        perror("shmget");
        exit(1);
    }
    // Dolaczamy sie do pamieci
    shdata = attachShm(shmid);

    // Inicjalizujemy dane w pamieci dzielonej
    shdata->kapitanStatkuPID = getpid();
    shdata->generatorPID = 0; // wypelni pozniej generator
    shdata->endOfDay = false;
    shdata->traveling = false;
    shdata->loading = true;
    shdata->disembarking = false;
    shdata->rejsCount = 0;

    // Zapisujemy parametry
    shdata->N = N_;
    shdata->K = K_;
    shdata->R = R_;
    shdata->T2= T2_;

    // Ustawiamy obsluge sygnalow SIGUSR1, SIGUSR2
    struct sigaction sa1, sa2;
    memset(&sa1,0,sizeof(sa1));
    sa1.sa_handler = sigusr1_handler;
    if(sigaction(SIGUSR1,&sa1,nullptr)==-1) {
        perror("sigaction1");
    }
    memset(&sa2,0,sizeof(sa2));
    sa2.sa_handler = sigusr2_handler;
    if(sigaction(SIGUSR2,&sa2,nullptr)==-1) {
        perror("sigaction2");
    }

    // Log startu
    {
        std::string msg = "[KapitanStatku] start. (K="+std::to_string(K_)+", N="+std::to_string(N_)+")";
        colorLog(msg, COL_BGREEN);
    }
    colorLog("[KapitanStatku] Zaladunek: czekam, az statek sie wypelni lub sygnal1. Jesli sygnal2 -> end.", COL_WHITE);

    // Glowna petla rejsow
    while(true) {
        // Jesli osiagnieto R rejsow lub endOfDay==true, konczymy
        if(shdata->rejsCount >= shdata->R || shdata->endOfDay) {
            break;
        }

        earlyDeparture=0;
        shdata->loading=true;
        shdata->disembarking=false;
        setSemValue(semid, SEM_DIR, 1); // 1=zaladunek

        // Czekamy az statek sie zapelni LUB wczesniejsze wyplyniecie LUB endOfDay
        while(true) {
            if(shdata->endOfDay) {
                colorLog("[KapitanStatku] Sygnal2 w trakcie zaladunku => forceUnload i koniec.", COL_RED);
                forceUnload();
                shdata->endOfDay = true; // zaznaczamy koniec
                goto KONIEC;
            }
            if(earlyDeparture) {
                colorLog("[KapitanStatku] Sygnal1 => wczesniejsze wyplyniecie (koncze zaladunek).", COL_CYAN);
                break;
            }
            int freeOnShip = getSemValue(semid, SEM_SHIP);
            if(freeOnShip == 0) {
                colorLog("[KapitanStatku] Statek jest pelny! Koncze zaladunek.", COL_GREEN);
                break;
            }
            usleep(200000);
        }
        // Koniec zaladunku
        shdata->loading=false;
        if(shdata->endOfDay) {
            break;
        }

        int passengersOnShip = N_ - getSemValue(semid, SEM_SHIP);
        {
            std::string msg = "[KapitanStatku] Koniec zaladunku. Na statku jest "+std::to_string(passengersOnShip)+" pasazerow.";
            colorLog(msg, COL_WHITE);
        }

        // Czekamy az mostek pusty
        while(true) {
            if(shdata->endOfDay) {
                colorLog("[KapitanStatku] Sygnal2 w trakcie czekania => forceUnload i koniec.", COL_RED);
                forceUnload();
                goto KONIEC;
            }
            int valB = getSemValue(semid, SEM_BRIDGE);
            if(valB==K_) break;
            usleep(100000);
        }

        // Wyplywamy w rejs
        shdata->traveling=true;
        {
            std::string msg = "[KapitanStatku] Wyplywam w rejs nr "+std::to_string(shdata->rejsCount+1)+
                              ", T2="+std::to_string(shdata->T2)+"s";
            colorLog(msg, COL_BBLUE);
        }
        sleep(shdata->T2);

        shdata->traveling=false;
        shdata->rejsCount++;
        {
            std::string msg = "[KapitanStatku] Wrocilem z rejsu nr "+std::to_string(shdata->rejsCount);
            colorLog(msg, COL_BBLUE);
        }

        if(shdata->endOfDay) {
            colorLog("[KapitanStatku] Sygnal2 w trakcie rejsu => dokonczyc i wyladowanie, koniec.", COL_RED);
            forceUnload();
            break;
        }

        // Wyladunek
        shdata->disembarking=true;
        setSemValue(semid, SEM_DIR, 0);
        colorLog("[KapitanStatku] Rozpoczynam wyladunek po rejsie...", COL_WHITE);

        // Czekamy az statek bedzie pusty (SEM_SHIP==N)
        while(true) {
            int freeOnShip = getSemValue(semid, SEM_SHIP);
            if(freeOnShip == N_) break;
            if(shdata->endOfDay) {
                colorLog("[KapitanStatku] Sygnal2 w trakcie wyladunku => forceUnload i koniec.", COL_RED);
                forceUnload();
                goto KONIEC;
            }
            usleep(200000);
        }
        shdata->disembarking=false;
        colorLog("[KapitanStatku] Zakonczylem wyladunek.", COL_WHITE);

        if(shdata->rejsCount >= shdata->R) {
            std::string msg = "[KapitanStatku] Osiagnieto R="+std::to_string(shdata->R)+". Koniec dnia.";
            colorLog(msg, COL_BRED);
            shdata->endOfDay=true;
            break;
        }
        colorLog("[KapitanStatku] Zaladunek: czekam, az statek sie wypelni lub sygnal1. Jesli sygnal2 -> end.", COL_WHITE);
    }

KONIEC:
    {
        std::string msg = "[KapitanStatku] Konczy. rejsCount="+std::to_string(shdata->rejsCount)+
                          " endOfDay="+std::to_string(shdata->endOfDay);
        colorLog(msg, COL_BRED);
    }

    // Czekamy, az generator sie zakonczy (kill(generatorPID, 0) => ESRCH)
    if(shdata->generatorPID > 0) {
        std::string msg = "[KapitanStatku] Czekam, az generator (pid="+std::to_string(shdata->generatorPID)+") sie zakonczy...";
        colorLog(msg, COL_GRAY);

        while(true) {
            // kill(pid,0) zwraca -1 z ESRCH, jesli procesu nie ma
            if(kill(shdata->generatorPID, 0)==-1 && errno==ESRCH) {
                break;
            }
            usleep(100000);
        }
    }

    // Usuwamy semafory i pamiec
    detachShm(shdata);
    if(shmctl(shmid, IPC_RMID, nullptr)==-1) {
        perror("shmctl IPC_RMID");
    }
    if(semctl(semid, 0, IPC_RMID)==-1) {
        perror("semctl IPC_RMID");
    }

    return 0;
}
