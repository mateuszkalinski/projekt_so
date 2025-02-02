#include "common.h"

// ------------------------- SEMAFORY --------------------------

int createOrGetSemaphore(key_t key) {
    // Tworzy tablice 3 semaforow: SEM_BRIDGE, SEM_SHIP, SEM_DIR
    int semid = semget(key, SEM_COUNT, 0666 | IPC_CREAT);
    if(semid == -1) {
        perror("semget");
        exit(1);
    }
    return semid;
}

int setSemValue(int semid, int semnum, int value) {
    union semun arg;
    arg.val = value;
    if(semctl(semid, semnum, SETVAL, arg) == -1) {
        perror("semctl SETVAL");
        return -1;
    }
    return 0;
}

int getSemValue(int semid, int semnum) {
    int val = semctl(semid, semnum, GETVAL);
    if(val == -1) {
        perror("semctl GETVAL");
    }
    return val;
}

// op < 0 => P (wait), op > 0 => V (signal)
void semOp(int semid, int semnum, int op) {
    struct sembuf sb;
    sb.sem_num = semnum;
    sb.sem_op = op;
    sb.sem_flg = 0;
    if (semop(semid, &sb, 1) == -1) {
        perror("semop");
    }
}

// -------------------- PAMIEC DZIELONA -------------------------

int createOrGetShm(key_t key) {
    int shmid = shmget(key, sizeof(SharedData), 0666 | IPC_CREAT);
    if(shmid == -1) {
        perror("shmget");
        exit(1);
    }
    return shmid;
}

SharedData* attachShm(int shmid) {
    void* addr = shmat(shmid, nullptr, 0);
    if(addr == (void*)-1) {
        perror("shmat");
        exit(1);
    }
    return reinterpret_cast<SharedData*>(addr);
}

void detachShm(const void* addr) {
    if(shmdt(addr) == -1) {
        perror("shmdt");
    }
}

void removeShm(int shmid) {
    if(shmctl(shmid, IPC_RMID, nullptr) == -1) {
        perror("shmctl(IPC_RMID)");
    }
}

//te
