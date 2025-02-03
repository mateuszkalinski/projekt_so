#include "common.h"
#include <cstdlib>
#include <ctime>

int main()
{
    logMsg("Kapitan Portu uruchomiony. (losowe sygnaly)");

    key_t keyShm = ftok(FTOK_PATH, FTOK_ID_SHM);
    int shmid = shmget(keyShm, sizeof(SharedData), 0666);
    if(shmid == -1) {
        perror("kapitanPortu shmget");
        exit(1);
    }
    SharedData* shdata = attachShm(shmid);

    while(shdata->kapitanStatkuPID == 0) {
        logMsg("Czekam na uruchomienie Kapitana Statku...");
        sleep(1);
    }
    pid_t pidStatku = shdata->kapitanStatkuPID;
    srand(time(NULL));

    while(true) {
        if(shdata->endOfDay || shdata->rejsCount >= R) {
            logMsg("Kapitan Portu konczy, bo endOfDay lub R osiagniete.");
            break;
        }
        int waitSec = 2 + (rand()%4); // 2..5
        sleep(waitSec);

        if(shdata->endOfDay || shdata->rejsCount >= R) {
            break;
        }

        int x = rand()%100;
        if(x < 30) {
            logMsg("Wysylam SIGUSR1 (wczesniejsze wyplyniecie). (x="+std::to_string(x)+")");
            kill(pidStatku, SIGUSR1);
        } else if(x < 40) {
            logMsg("Wysylam SIGUSR2 (koniec rejsow). (x="+std::to_string(x)+")");
            kill(pidStatku, SIGUSR2);
        } else {
            logMsg("Nie wysylam sygnalu tym razem. (x="+std::to_string(x)+")");
        }
    }

    detachShm(shdata);
    logMsg("Kapitan Portu zakonczyl dzialanie.");
    return 0;
}
