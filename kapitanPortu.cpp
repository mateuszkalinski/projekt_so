#include "common.h"
#include <cstdlib>
#include <ctime>

int main()
{
    key_t keySem = ftok(FTOK_PATH, FTOK_ID_SEM);
    key_t keyShm = ftok(FTOK_PATH, FTOK_ID_SHM);

    int semid = semget(keySem, SEM_COUNT, 0666);
    if(semid == -1) {
        perror("kapitanPortu semget");
        exit(1);
    }
    int shmid = shmget(keyShm, sizeof(SharedData), 0666);
    if(shmid == -1) {
        perror("kapitanPortu shmget");
        exit(1);
    }
    SharedData* shdata = attachShm(shmid);

    logMsg("Kapitan Portu uruchomiony.");

    // Czekamy az kapitanStatkuPID bedzie ustawiony
    while(shdata->kapitanStatkuPID == 0) {
        logMsg("Oczekuje na uruchomienie Kapitana Statku...");
        sleep(1);
    }
    pid_t pidStatku = shdata->kapitanStatkuPID;

    logMsg("PID Kapitana Statku: " + std::to_string(pidStatku));

    srand(time(nullptr));

    // Pierwszy sygnal1 wysylamy np. po 5-10s
    int wait1 = 5 + (rand() % 6);
    logMsg("Za " + std::to_string(wait1) + "s wysle SIGUSR1 (wczesniejsze wyplyniecie).");
    sleep(wait1);

    kill(pidStatku, SIGUSR1);
    logMsg("Wyslano SIGUSR1.");

    // Drugi sygnal2 po kolejnym 5-10s
    int wait2 = 5 + (rand() % 6);
    logMsg("Za " + std::to_string(wait2) + "s wysle SIGUSR2 (koniec rejsow).");
    sleep(wait2);

    kill(pidStatku, SIGUSR2);
    logMsg("Wyslano SIGUSR2.");

    detachShm(shdata);
    return 0;
}
