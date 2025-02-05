#include "common.h"
#include <cstring>

// -----------------------------
// Globalne zmienne i flagi
// -----------------------------

// Ta flaga ustawia sie na 1, gdy otrzymamy sygnal SIGUSR1 (wczesniejszy rejs)
static volatile sig_atomic_t earlyDeparture = 0;

// Ta flaga ustawia sie, gdy otrzymamy SIGINT (np. Ctrl+C)
static volatile sig_atomic_t ctrlC = 0;

// Identyfikatory zasobow systemowych (semaforow, pamieci dzielonej)
static int semid = -1;
static int shmid = -1;

// Wskaznik do danych w pamieci dzielonej
static SharedData* shdata = nullptr;

// Zabezpieczenie, zeby nie usuwac zasobow kilka razy
static bool resourcesCleaned = false;


// -----------------------------
// Handlery sygnalow
// -----------------------------

// Sygnal1 (SIGUSR1) => wczesniejsze wyplyniecie (jesli trwa zaladunek)
void sigusr1_handler(int)
{
    earlyDeparture = 1;
}

// Sygnal2 (SIGUSR2) => endOfDay=true (konczymy rejsy)
void sigusr2_handler(int)
{
    if(shdata) {
        shdata->endOfDay = true;
    }
}

// SIGINT (np. Ctrl+C) => ustaw endOfDay i ctrlC
void sigint_handler(int)
{
    colorLog("[KapitanStatku] Otrzymalem SIGINT, ustawiam endOfDay.", COL_RED);
    if(shdata) {
        shdata->endOfDay = true;
    }
    ctrlC = 1;
}


// -----------------------------
// Funkcje pomocnicze
// -----------------------------

// forceUnload()
// Natychmiast wyrzuca wszystkich pasazerow ze statku i mostka
// (zwieksza semafory, aby staly sie maksymalne)
void forceUnload()
{
    colorLog("[KapitanStatku] forceUnload: wymuszam wyladowanie pasazerow!", COL_BRED);
    if(!shdata) return; // jesli cos jest nie tak z pamiecia, nic nie robimy

    // Wylaczamy zaladunek, wlaczamy wyladunek
    shdata->loading = false;
    shdata->disembarking = true;

    // SEM_DIR=0 oznacza wykladanie
    setSemValue(semid, SEM_DIR, 0);

    // Zwiekszamy SEM_SHIP dopoki (SEM_SHIP == N), co znaczy pusty statek
    while(true) {
        int freeOnShip = getSemValue(semid, SEM_SHIP);
        if(freeOnShip == shdata->N) {
            break;
        }
        semOp(semid, SEM_SHIP, +1);
        usleep(100000);
    }

    // Zwiekszamy SEM_BRIDGE dopoki (SEM_BRIDGE == K), co oznacza pusty mostek
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

// cleanupResources()
// Usuwa semafory i pamiec dzielona tylko raz (dzieki resourcesCleaned)
void cleanupResources()
{
    if(!resourcesCleaned) {
        resourcesCleaned = true; 
        colorLog("[KapitanStatku] Czyszcze zasoby IPC (sem, shm)...", COL_GRAY);

        // Odlaczamy sie od pamieci dzielonej
        if(shdata) {
            detachShm(shdata);
            shdata = nullptr;
        }

        // Usuwamy pamiec dzielona (shmctl IPC_RMID)
        if(shmid != -1) {
            if(shmctl(shmid, IPC_RMID, nullptr)==-1) {
                perror("shmctl IPC_RMID");
            }
            shmid = -1;
        }

        // Usuwamy semafory (semctl IPC_RMID)
        if(semid != -1) {
            if(semctl(semid, 0, IPC_RMID)==-1) {
                perror("semctl IPC_RMID");
            }
            semid = -1;
        }
    }
}


// -----------------------------
// MAIN - Glowna funkcja
// -----------------------------
int main(int argc, char* argv[])
{
    // Sprawdzamy czy podano 4 parametry: N, K, R, T2
    if(argc < 5) {
        std::cerr << "Uzycie: " << argv[0] << " <N> <K> <R> <T2>\n";
        exit(1);
    }
    int N_ = atoi(argv[1]);
    int K_ = atoi(argv[2]);
    int R_ = atoi(argv[3]);
    int T2_ = atoi(argv[4]);

    // Prosta walidacja parametrow
    if(N_<=0 || K_<=0 || R_<=0 || T2_<=0) {
        std::cerr << "Parametry musza byc > 0!\n";
        exit(1);
    }
    if(K_ >= N_) {
        std::cerr << "K < N musi byc!\n";
        exit(1);
    }

    // Rejestrujemy obsluge sygnalow SIGUSR1, SIGUSR2, SIGINT
    {
        struct sigaction sa1, sa2, sa3;
        memset(&sa1, 0, sizeof(sa1));
        sa1.sa_handler = sigusr1_handler;
        sigaction(SIGUSR1, &sa1, nullptr);

        memset(&sa2, 0, sizeof(sa2));
        sa2.sa_handler = sigusr2_handler;
        sigaction(SIGUSR2, &sa2, nullptr);

        memset(&sa3, 0, sizeof(sa3));
        sa3.sa_handler = sigint_handler;
        sigaction(SIGINT, &sa3, nullptr);
    }

    // Tworzymy klucze do semaforow i pamieci
    key_t keySem = ftok(FTOK_PATH, 0x11);
    if(keySem == -1) { perror("ftok sem"); exit(1); }
    key_t keyShm = ftok(FTOK_PATH, 0x12);
    if(keyShm == -1) { perror("ftok shm"); exit(1); }

    // Tworzymy semafory z flaga IPC_CREAT
    semid = semget(keySem, SEM_COUNT, 0600 | IPC_CREAT);
    if(semid == -1) {
        perror("semget");
        exit(1);
    }
    // Ustawiamy startowe wartosci semaforow
    setSemValue(semid, SEM_BRIDGE, K_);
    setSemValue(semid, SEM_SHIP,   N_);
    setSemValue(semid, SEM_DIR,    1);

    // Tworzymy/pobieramy pamiec dzielona
    shmid = shmget(keyShm, sizeof(SharedData), 0600 | IPC_CREAT);
    if(shmid == -1) {
        perror("shmget");
        // Jesli nie udalo sie utworzyc pamieci, sprzatamy semafory
        cleanupResources();
        exit(1);
    }
    shdata = attachShm(shmid);

    // Nadpisujemy dane w pamieci dzielonej - na wypadek, gdyby cos starego tam bylo
    shdata->kapitanStatkuPID = getpid();
    shdata->generatorPID = 0;      // ustawi to generator
    shdata->endOfDay = false;
    shdata->traveling = false;
    shdata->loading = true;
    shdata->disembarking = false;
    shdata->rejsCount = 0;
    shdata->N = N_;
    shdata->K = K_;
    shdata->R = R_;
    shdata->T2= T2_;

    // Log o starcie
    {
        std::string msg = "[KapitanStatku] start. (K="+std::to_string(K_)+", N="+std::to_string(N_)+")";
        colorLog(msg, COL_BGREEN);
    }
    colorLog("[KapitanStatku] Zaladunek: czekam, az statek sie wypelni lub sygnal1. Jesli sygnal2 -> end.", COL_WHITE);

    // Czekamy, az generator (pasazer.cpp) uruchomi sie i ustawi generatorPID
    while(!shdata->endOfDay && !ctrlC && shdata->generatorPID == 0) {
        usleep(100000);
    }

    // Glowna petla rejsow:
    while(true) {
        // Jesli przekroczono liczbe R, ustaw endOfDay
        if(shdata->rejsCount >= shdata->R) {
            shdata->endOfDay = true;
            break;
        }
        // Jesli sygnal2 lub Ctrl+C, wychodzimy
        if(shdata->endOfDay || ctrlC) {
            break;
        }

        // Start zaladunku
        earlyDeparture=0;
        shdata->loading = true;
        shdata->disembarking = false;
        setSemValue(semid, SEM_DIR, 1);

        // Czekamy az statek sie zapelni LUB sygnal1 LUB sygnal2
        while(true) {
            if(shdata->endOfDay || ctrlC) {
                colorLog("[KapitanStatku] Sygnal2 w trakcie zaladunku => forceUnload i koniec.", COL_RED);
                forceUnload();
                shdata->endOfDay = true;
                goto FINISH;
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

        // Zaladunek konczy sie
        shdata->loading = false;
        if(shdata->endOfDay || ctrlC) {
            break;
        }

        // Ile pasazerow jest na statku
        int passengersOnShip = N_ - getSemValue(semid, SEM_SHIP);
        {
            std::string msg = "[KapitanStatku] Koniec zaladunku. Na statku jest "+std::to_string(passengersOnShip)+" pasazerow.";
            colorLog(msg, COL_WHITE);
        }

        // Czekamy, az mostek bedzie pusty (SEM_BRIDGE==K)
        while(true) {
            if(shdata->endOfDay || ctrlC) {
                colorLog("[KapitanStatku] Sygnal2 w trakcie czekania => forceUnload i koniec.", COL_RED);
                forceUnload();
                shdata->endOfDay = true;
                goto FINISH;
            }
            int valB = getSemValue(semid, SEM_BRIDGE);
            if(valB == K_) break;
            usleep(100000);
        }

        // Wyplywamy w rejs
        shdata->traveling = true;
        {
            std::string msg = "[KapitanStatku] Wyplywam w rejs nr "+std::to_string(shdata->rejsCount+1)+
                              ", T2="+std::to_string(shdata->T2)+"s";
            colorLog(msg, COL_BBLUE);
        }
        sleep(shdata->T2);

        // Koniec rejsu
        shdata->traveling = false;
        shdata->rejsCount++;
        {
            std::string msg = "[KapitanStatku] Wrocilem z rejsu nr "+std::to_string(shdata->rejsCount);
            colorLog(msg, COL_BBLUE);
        }

        // Jesli sygnal2 po rejsie, to wyrzucamy pasazerow i konczymy
        if(shdata->endOfDay || ctrlC) {
            colorLog("[KapitanStatku] Sygnal2 w trakcie rejsu => dokonczyc i wyladowanie, koniec.", COL_RED);
            forceUnload();
            shdata->endOfDay = true;
            break;
        }

        // Wyladunek
        shdata->disembarking = true;
        setSemValue(semid, SEM_DIR, 0);
        colorLog("[KapitanStatku] Rozpoczynam wyladunek po rejsie...", COL_WHITE);

        while(true) {
            int freeOnShip = getSemValue(semid, SEM_SHIP);
            if(freeOnShip == N_) break; 
            if(shdata->endOfDay || ctrlC) {
                colorLog("[KapitanStatku] Sygnal2 w trakcie wyladunku => forceUnload i koniec.", COL_RED);
                forceUnload();
                shdata->endOfDay = true;
                goto FINISH;
            }
            usleep(200000);
        }

        shdata->disembarking = false;
        colorLog("[KapitanStatku] Zakonczylem wyladunek.", COL_WHITE);
    }

// Etykieta FINISH - konczymy program
FINISH:
    {
        std::string msg = "[KapitanStatku] Konczy. rejsCount="+std::to_string(shdata->rejsCount)+
                          " endOfDay="+std::to_string(shdata->endOfDay);
        colorLog(msg, COL_BRED);
    }

    // Jesli mamy generatorPID, czekamy az generator sie zakonczy (kill(...0) => ESRCH = brak procesu)
    if(shdata->generatorPID > 0) {
        std::string msg = "[KapitanStatku] Czekam, az generator (pid="+std::to_string(shdata->generatorPID)+") sie zakonczy...";
        colorLog(msg, COL_GRAY);

        while(true) {
            if(kill(shdata->generatorPID, 0)==-1 && errno==ESRCH) {
                break;
            }
            usleep(100000);
        }
    }

    // Na samym koncu usuwamy semafory i pamiec (jesli jeszcze nie byly usuniete)
    cleanupResources();

    return 0;
}
