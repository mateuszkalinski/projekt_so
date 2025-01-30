#include "common.h"
#include <vector>
#include <cstdlib>

// Funkcja w procesie potomnym (pasazer)
void passengerProcess(int id)
{
    // Dolaczenie do semaforow i pamieci
    key_t keySem = ftok(FTOK_PATH, FTOK_ID_SEM);
    key_t keyShm = ftok(FTOK_PATH, FTOK_ID_SHM);

    int semid = semget(keySem, SEM_COUNT, 0666);
    if(semid == -1) {
        perror("pasazer semget");
        exit(1);
    }
    int shmid = shmget(keyShm, sizeof(SharedData), 0666);
    if(shmid == -1) {
        perror("pasazer shmget");
        exit(1);
    }

    SharedData* shdata = attachShm(shmid);

    // Proba wejscia na mostek
    // Najpierw sprawdzamy, czy statek jest w fazie disembarking. Jesli tak, 
    // to mozemy troche poczekac, az skonczy lub wyjdziemy. 
    // (zeby pokazac unikanie kolizji kierunkow)
    while(shdata->disembarking) {
        // czekamy
        sleep(1);
        if(shdata->endOfDay) {
            logMsg("Pasazer " + std::to_string(id) + " rezygnuje, bo endOfDay w trakcie czekania na kierunek.");
            detachShm(shdata);
            exit(0);
        }
    }

    // Sprawdzamy, czy trwa zaladunek. Jesli nie ma zaladunku i traveling==true, 
    // to i tak nie wejdziemy.
    if(!shdata->loading || shdata->traveling || shdata->endOfDay) {
        logMsg("Pasazer " + std::to_string(id) + " nie moze wejsc, bo statek nie jest w fazie zaladunku lub endOfDay.");
        detachShm(shdata);
        exit(0);
    }

    logMsg("Pasazer " + std::to_string(id) + " probuje wejsc na mostek...");
    semOp(semid, SEM_BRIDGE, -1); // P na mostek

    // Jesli w trakcie wchodzenia nadszedl endOfDay i statek nie plynie
    if(shdata->endOfDay && !shdata->traveling) {
        logMsg("Pasazer " + std::to_string(id) + " rezygnuje, bo endOfDay podczas wchodzenia na mostek.");
        semOp(semid, SEM_BRIDGE, 1); // zwalniamy mostek
        detachShm(shdata);
        exit(0);
    }

    logMsg("Pasazer " + std::to_string(id) + " jest na moscie.");

    // Wejscie na statek (P na SEM_SHIP)
    semOp(semid, SEM_SHIP, -1);
    // Zwolnienie mostka
    semOp(semid, SEM_BRIDGE, 1);

    // Jesli w tym momencie ogloszono endOfDay, a nie wyruszylismy 
    // (traveling==false), to schodzimy:
    if(shdata->endOfDay && !shdata->traveling) {
        logMsg("Pasazer " + std::to_string(id) + " schodzi, bo endOfDay i statek nie wyruszyl.");
        semOp(semid, SEM_SHIP, 1); // oddajemy miejsce
        detachShm(shdata);
        exit(0);
    }

    logMsg("Pasazer " + std::to_string(id) + " jest na statku. Czeka na rejs.");

    // Czekamy na rejs: statek zmieni traveling=false->true->false. 
    // Ale jest mozliwe, ze statek jest juz traveling (jesli zdazyl),
    // to czekamy do konca rejsu.
    while(!shdata->endOfDay && !shdata->traveling) {
        sleep(1);
    }
    // Teraz statek w rejsie
    while(!shdata->endOfDay && shdata->traveling) {
        sleep(1);
    }

    // Po zakonczeniu rejsu schodzimy
    logMsg("Pasazer " + std::to_string(id) + " schodzi ze statku po rejsie.");
    semOp(semid, SEM_SHIP, 1);

    detachShm(shdata);
    exit(0);
}

int main(int argc, char* argv[])
{
    if(argc < 2) {
        std::cerr << "Uzycie: " << argv[0] << " <liczba_pasazerow>\n";
        return 1;
    }
    int M = std::atoi(argv[1]);
    logMsg("Proces PASAZER: tworze " + std::to_string(M) + " pasazerow.");

    std::vector<pid_t> pids;
    pids.reserve(M);

    for(int i = 0; i < M; i++) {
        pid_t pid = fork();
        if(pid == 0) {
            // child
            passengerProcess(i+1);
        } else if(pid > 0) {
            pids.push_back(pid);
        } else {
            perror("fork");
        }
    }

    // Czekamy na wszystkie procesy potomne
    for(pid_t pid : pids) {
        waitpid(pid, nullptr, 0);
    }

    logMsg("Wszyscy pasazerowie zakonczyli dzialanie. Koncze pasazer.cpp.");
    return 0;
}
//test