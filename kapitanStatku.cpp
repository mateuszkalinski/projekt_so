#include "common.h"

static volatile sig_atomic_t earlyDeparture = 0; // sygnal1 flaga

// Semafory i pamiec dzielona
static int semid = -1;
static int shmid = -1;
static SharedData* shdata = nullptr;

// Handler sygnalu1
void sigusr1_handler(int sig) {
    earlyDeparture = 1; 
}

// Handler sygnalu2
void sigusr2_handler(int sig) {
    if(shdata) {
        shdata->endOfDay = true;
    }
}

int main() 
{
    // Utworzenie/pobranie semaforow
    key_t keySem = ftok(FTOK_PATH, FTOK_ID_SEM);
    key_t keyShm = ftok(FTOK_PATH, FTOK_ID_SHM);

    semid = createOrGetSemaphore(keySem);
    shmid = createOrGetShm(keyShm);

    // Dolaczamy pamiec dzielona
    shdata = attachShm(shmid);

    // Inicjalizacja semaforow (dla uproszczenia zawsze, 
    // mozna pomyslec o testowaniu czy nowo utworzone)
    setSemValue(semid, SEM_BRIDGE, K); 
    setSemValue(semid, SEM_SHIP,   N);
    setSemValue(semid, SEM_DIR,    1);

    // Ustawienia stanu poczatkowego
    shdata->kapitanStatkuPID = getpid();
    shdata->endOfDay       = false;
    shdata->traveling      = false;
    shdata->loading        = false;
    shdata->disembarking   = false;
    shdata->rejsCount      = 0;

    logMsg("Kapitan Statku: uruchomiono. Ustawiam semafory (mostek=" + 
           std::to_string(K) + ", statek=" + std::to_string(N) + ")");

    // Ustawienie handlerow sygnalow
    struct sigaction sa1, sa2;
    memset(&sa1, 0, sizeof(sa1));
    sa1.sa_handler = sigusr1_handler;
    sigaction(SIGUSR1, &sa1, nullptr);

    memset(&sa2, 0, sizeof(sa2));
    sa2.sa_handler = sigusr2_handler;
    sigaction(SIGUSR2, &sa2, nullptr);

    // Glowna petla
    while(true) {
        if(shdata->rejsCount >= R || shdata->endOfDay) {
            break;
        }

        // Rozpoczynamy zaladunek
        shdata->loading = true;
        earlyDeparture  = 0;

        logMsg("Zaladunek pasazerow, czekam do " + std::to_string(T1) + 
               " s lub sygnalu1...");

        // Czekamy max T1, w tym czasie sprawdzamy w petli czy endOfDay lub earlyDeparture
        for(int i = 0; i < T1; i++) {
            sleep(1);
            if(earlyDeparture || shdata->endOfDay) {
                break;
            }
        }

        // Jesli w trakcie czekania nadszedl sygnal2 -> endOfDay
        // to nie wyplywamy
        if(shdata->endOfDay) {
            logMsg("Przerwano rejs, bo endOfDay w trakcie zaladunku.");
            shdata->loading = false;
            break;
        }

        // Teraz czekamy, az mostek bedzie pusty (wartosc semafora SEM_BRIDGE == K)
        // bo nie mozemy wyplynac, gdy ktos jest na moscie
        while(true) {
            int valBridge = getSemValue(semid, SEM_BRIDGE);
            if(valBridge == K) {
                break;
            }
            sleep(1);
            // jesli nagle endOfDay w tym czasie, to rezygnujemy
            if(shdata->endOfDay) {
                logMsg("Przerwano rejs, bo endOfDay, a ludzie wciaz na moscie.");
                shdata->loading = false;
                break;
            }
        }

        shdata->loading = false;
        if(shdata->endOfDay) {
            break;
        }

        // Teraz wyplywamy
        shdata->traveling = true;
        logMsg("Wyplywam w rejs nr " + std::to_string(shdata->rejsCount+1) + 
               ". (czas rejsu = " + std::to_string(T2) + " s)");

        sleep(T2);

        // Koniec rejsu
        shdata->traveling = false;
        shdata->rejsCount++;
        logMsg("Wrocilem z rejsu nr " + std::to_string(shdata->rejsCount));

        // Sprawdzamy, czy w miedzyczasie nadszedl sygnal2
        // Jesli tak, to konczymy prace, ale najpierw pasazerowie schodza.
        // "disembarking" - sygnalizujemy pasazerom, ze moga schodzic
        shdata->disembarking = true;
        logMsg("Rozpoczynam wyadunek pasazerow...");

        // W prosty sposob dajemy im chwile, by wyszli, np. 3 sek
        // (pasazer i tak schodzi, gdy zauwazy traveling=false).
        sleep(3);

        // Konczymy wyadunek
        shdata->disembarking = false;
        logMsg("Zakonczylem wyadunek.");

        if(shdata->rejsCount >= R) {
            logMsg("Osiagnieto maksymalna liczbe rejsow (" + std::to_string(R) + 
                   "). Koncze dzien.");
            shdata->endOfDay = true;
            break;
        }

        if(shdata->endOfDay) {
            // Gdy sygnal2 nadszedl podczas rejsu, docieramy tu i konczymy
            break;
        }
    }

    logMsg("Kapitan Statku konczy prace. rejsCount=" + 
           std::to_string(shdata->rejsCount) + 
           ", endOfDay=" + std::to_string(shdata->endOfDay));

    // Opcjonalnie czekamy chwile, aby pasazerowie mogli zakonczyc
    sleep(2);

    // Tu mozemy usunac semafory i pamiec (jesli zakladamy, ze kapitan konczy jako ostatni)
    // lub pozostawic je do ewentualnego recznego czyszczenia.
    // removeShm(shmid);
    // semctl(semid, 0, IPC_RMID);

    // Odczepiamy pamiec
    detachShm(shdata);

    return 0;
}
