#include "common.h"
#include <cstdlib>
#include <ctime>

int main()
{
    colorLog("[KapitanPortu] uruchomiony. (losowe sygnaly)", COL_BGREEN);

    key_t keyShm = ftok(FTOK_PATH, 0x12);
    if(keyShm==-1) { perror("ftok kapitanPortu shm"); exit(1); }
    int shmid = shmget(keyShm, sizeof(SharedData), 0600);
    if(shmid == -1) {
        perror("kapitanPortu shmget");
        exit(1);
    }
    SharedData* shdata = attachShm(shmid);

    // czekamy az kapitanStatkuPID bedzie ustawione
    while(shdata->kapitanStatkuPID == 0) {
        colorLog("[KapitanPortu] czekam na uruchomienie Kapitana Statku...", COL_GRAY);
        sleep(1);
    }
    pid_t pidStatku = shdata->kapitanStatkuPID;
    srand(time(NULL));

    while(true) {
        if(shdata->endOfDay || shdata->rejsCount >= shdata->R) {
            colorLog("[KapitanPortu] konczy, bo endOfDay lub R osiagniete.", COL_BRED);
            break;
        }
        int waitSec = 2 + (rand()%3); // 2..4
        sleep(waitSec);

        if(shdata->endOfDay || shdata->rejsCount >= shdata->R) {
            break;
        }

        int x = rand()%100;
        if(x<90) {
            std::string msg = "[KapitanPortu] Wysylam SIGUSR1 (wczesniejsze wyplyniecie). (x="+std::to_string(x)+")";
            colorLog(msg, COL_BCYAN);
            kill(pidStatku, SIGUSR1);
        } else if(x<95) {
            std::string msg = "[KapitanPortu] Wysylam SIGUSR2 (koniec rejsow). (x="+std::to_string(x)+")";
            colorLog(msg, COL_BRED);
            kill(pidStatku, SIGUSR2);
        } else {
            std::string msg = "[KapitanPortu] Nie wysylam sygnalu tym razem. (x="+std::to_string(x)+")";
            colorLog(msg, COL_GRAY);
        }
    }

    detachShm(shdata);
    colorLog("[KapitanPortu] zakonczyl dzialanie.", COL_BRED);
    return 0;
}