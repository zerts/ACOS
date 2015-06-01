#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/mman.h>

extern int errno;

int numberOfPhilosophers;
int *cnts;
int *numberOfEatenParts;
pid_t *pids;
pid_t *pid;
int hasLeftStick, hasRightStick;
int leftStick, rightStick;
int philosopherNumber;
int *beginPosOfStick;

void detectError(char * err)
{
    perror(err);
    errno = 0;
}

void getStick(int sig)
{
    if (sig == SIGUSR1)
    {
        printf("philosopher #%d got left stick\n", philosopherNumber);
        hasLeftStick = 1;
    }
    else if (sig == SIGUSR2)
    {
        printf("philosopher #%d got rigth stick\n", philosopherNumber);
        hasRightStick = 1;
    }
    else
    {
        exit(0);
    }
    if (!hasLeftStick || !hasRightStick)
        return;
    printf("philosopher #%d start eating\n", philosopherNumber);
    ++numberOfEatenParts[philosopherNumber];
    sleep(1);

    printf("table of number of eaten parts: ");
    for (int i = 0; i < numberOfPhilosophers; ++i)
    {
        printf("#%d-%d ", i, numberOfEatenParts[i]);
    }
    printf("\n");
    if (rand() % 2)
    {
        hasRightStick = 0;
        kill(pid[(philosopherNumber + 1) % numberOfPhilosophers], SIGUSR1);
        hasLeftStick = 0;
        kill(pid[(philosopherNumber + numberOfPhilosophers - 1) % numberOfPhilosophers], SIGUSR2);
    }
    else
    {
        hasLeftStick = 0;
        kill(pid[(philosopherNumber + numberOfPhilosophers - 1) % numberOfPhilosophers], SIGUSR2);
        hasRightStick = 0;
        kill(pid[(philosopherNumber + 1) % numberOfPhilosophers], SIGUSR1);
    }
}

void solve()
{
    struct sigaction step;
    printf("philosopher #%d start the game\n", philosopherNumber);
    leftStick = philosopherNumber;
    rightStick = (philosopherNumber + 1) % numberOfPhilosophers;
    step.sa_handler = getStick;
    sigemptyset(&step.sa_mask);
    sigaddset(&step.sa_mask, SIGUSR1);
    sigaction(SIGUSR1, &step, NULL);
    sigaddset(&step.sa_mask, SIGUSR2);
    sigaction(SIGUSR2, &step, NULL);

    if (beginPosOfStick[leftStick] == philosopherNumber)
        getStick(SIGUSR1);
    if (beginPosOfStick[rightStick] == philosopherNumber)
        getStick(SIGUSR2);

    while (1)
        sleep(1000);
}

void createSharedMemory()
{
    shm_unlink("phil_cnts");
    int fdCnts = shm_open("phil_cnts", O_RDWR | O_CREAT, 0777);
    shm_unlink("phil_pids");
    int fdPids = shm_open("phil_pids", O_RDWR | O_CREAT, 0777);
    if (fdCnts < 0 || fdPids < 0)
    {
        detectError("shm_open failed");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < numberOfPhilosophers; ++i)
    {
        if (write(fdCnts, cnts + i, sizeof(int)) != sizeof(int))
        {
            detectError("write failed");
            exit(EXIT_FAILURE);
        }
        if (write(fdPids, pids + i, sizeof(pid_t)) != sizeof(pid_t))
        {
            detectError("write failed");
            exit(EXIT_FAILURE);
        }
    }
    numberOfEatenParts = (int *) mmap(NULL, sizeof(int) * numberOfPhilosophers, PROT_WRITE, MAP_SHARED, fdCnts, 0);
    pid = (pid_t *) mmap(NULL, sizeof(int) * numberOfPhilosophers, PROT_WRITE, MAP_SHARED, fdPids, 0);
    if (numberOfEatenParts == MAP_FAILED || pid == MAP_FAILED)
    {
        detectError("mmap failed");
        exit(EXIT_FAILURE);
    }
    close(fdCnts);
    close(fdPids);
}

int main(int argc, char ** argv)
{
    numberOfPhilosophers = atoi(argv[1]);
    cnts = malloc(numberOfPhilosophers * sizeof(int));
    pids = malloc(numberOfPhilosophers * sizeof(int));
    pid_t t;
    beginPosOfStick = malloc(numberOfPhilosophers * sizeof(int));
    while (1)
    {
        int correctBegin = 0;
        for (int i = 0; i < numberOfPhilosophers; ++i)
            beginPosOfStick[i] = (i - rand() % 2 + numberOfPhilosophers) % numberOfPhilosophers;
        for (int i = 0; i < numberOfPhilosophers; ++i)
            if (beginPosOfStick[i] == i && beginPosOfStick[(i + 1) % numberOfPhilosophers] == i)
                correctBegin = 1;
        if (correctBegin)
            break;
    }
    for (int i = 0; i < numberOfPhilosophers; ++i)
        printf("stick #%d is in philosopher's #%d hands\n", i, beginPosOfStick[i]);
    createSharedMemory();
    for (philosopherNumber = 0; philosopherNumber < numberOfPhilosophers - 1; ++philosopherNumber)
    {
        t = fork();
        if (t == 0)
            break;
        else
            pid[philosopherNumber] = t;
    }
    if (philosopherNumber == numberOfPhilosophers - 1)
        pid[numberOfPhilosophers - 1] = getpid();
    solve();
    free(cnts);
    free(pids);
    return 0;
}
