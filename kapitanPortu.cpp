#include "common.h"
#include <cstdlib>
#include <ctime>

// Kapitan Portu - wysyla sygnaly SIGUSR1 (wczesniejsze wyplyniecie) i SIGUSR2 (koniec rejsow)
// w sposob losowy (co kilka sekund). Konczy dzialanie, gdy endOfDay=true lub rejsCount >= R.

int main()
{
    colorLog("[KapitanPortu] uruchomiony. (losowe sygnaly)", COL_BGREEN);

    // Tworzymy klucz do pamieci dzielonej
    key_t keyShm = ftok(FTOK_PATH, 0x12);
    if(keyShm==-1) { perror("ftok kapitanPortu shm"); exit(1); }

    // Proba pobrania juz istniejacego segmentu pamieci
    int shmid = shmget(keyShm, sizeof(SharedData), 0600);
    if(shmid == -1) {
        perror("kapitanPortu shmget");
        exit(1);
    }

    // Dolaczamy sie do pamieci dzielonej
    SharedData* shdata = attachShm(shmid);

    // Czekamy, az kapitanStatkuPID bedzie ustawione (KapitanStatku musi byc pierwszy)
    while(shdata->kapitanStatkuPID == 0) {
        colorLog("[KapitanPortu] czekam na uruchomienie Kapitana Statku...", COL_GRAY);
        sleep(1);
    }

    pid_t pidStatku = shdata->kapitanStatkuPID;
    srand(time(NULL)); // losowe sygnaly

    // Petla wysylania sygnalow, dopoki endOfDay==false i rejsCount<R
    while(true) {
        if(shdata->endOfDay || shdata->rejsCount >= shdata->R) {
            colorLog("[KapitanPortu] konczy, bo endOfDay lub R osiagniete.", COL_BRED);
            break;
        }

        // Odczekaj 2..4 sek
        int waitSec = 2 + (rand()%3);
        sleep(waitSec);

        if(shdata->endOfDay || shdata->rejsCount >= shdata->R) {
            break;
        }

        // Losujemy x i decydujemy, czy wyslac SIGUSR1, SIGUSR2, czy nic.
        int x = rand()%100;
        if(x<30) {
            // SIGUSR1
            std::string msg = "[KapitanPortu] Wysylam SIGUSR1 (wczesniejsze wyplyniecie). (x="+std::to_string(x)+")";
            colorLog(msg, COL_BCYAN);
            kill(pidStatku, SIGUSR1);
        } else if(x<40) {
            // SIGUSR2
            std::string msg = "[KapitanPortu] Wysylam SIGUSR2 (koniec rejsow). (x="+std::to_string(x)+")";
            colorLog(msg, COL_BRED);
            kill(pidStatku, SIGUSR2);
        } else {
            std::string msg = "[KapitanPortu] Nie wysylam sygnalu tym razem. (x="+std::to_string(x)+")";
            colorLog(msg, COL_GRAY);
        }
    }

    // Odlaczamy sie od pamieci dzielonej
    detachShm(shdata);
    colorLog("[KapitanPortu] zakonczyl dzialanie.", COL_BRED);
    return 0;
}
