#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <string.h>
#include <math.h>

#define MAX_NUMBER_OF_PROC 1 << 16

extern int errno;

void detectError(char * err)
{
    perror(err);
    errno = 0;
}

int charIsNumber (char c)
{
    if (c <= '9' && c >= '0')
        return 1;
    return 0;
}

struct process
{
    int number;
    long unsigned int usedMemory;
    double timeUsed;
    char * name;
};

char * path;
struct process processes[MAX_NUMBER_OF_PROC];
int numberOfProc;

void printProc(int i, struct process currProc)
{
    printf("#%2d ", i);
    printf("%4d ", currProc.number);
    printf("%20s ", currProc.name);
    printf("%.3f ", currProc.timeUsed);
    printf("%lu\n", currProc.usedMemory);
}

void getInformationAboutProcess(struct dirent * currProc)
{
    path = malloc((12 + strlen(currProc->d_name)) * sizeof(char));
    sprintf(path, "/proc/%s/stat", currProc->d_name);
    FILE * stats = fopen(path, "r");
    processes[numberOfProc].name = malloc(30 * sizeof(char));
    fscanf(stats, "%d %s", &(processes[numberOfProc].number), processes[numberOfProc].name);
    //printf("%d %s\n", processes[numberOfProc].number, processes[numberOfProc].name);
    char * uselessInfo = malloc(30 * sizeof(char));
    for (int i = 3; i < 14; i++)
        fscanf(stats, "%s", uselessInfo);
    long unsigned int utime, stime;
    fscanf(stats, "%lu %lu", &utime, &stime);
    processes[numberOfProc].timeUsed = (double)((utime + stime) / sysconf(_SC_CLK_TCK));
    for (int i = 16; i < 23; i++)
        fscanf(stats, "%s", uselessInfo);
    fscanf(stats, "%lu ", &(processes[numberOfProc].usedMemory));
    //printProc(processes[numberOfProc]);
    fclose(stats);
}

int compareProcess (const void * first, const void * second)
{
    if (fabs(((struct process *)first)->timeUsed - ((struct process *)second)->timeUsed) <= 1e-9)
        return 0;
    if (((struct process *)first)->timeUsed > ((struct process *)second)->timeUsed)
        return -1;
    return 1;
}

int main()
{
    DIR * dir;
	struct dirent * proc;
    dir = opendir("/proc");
	if (dir == NULL)
	{
        detectError("opendir failed");
        return 0;
	}
	numberOfProc = 0;
	while (proc = readdir(dir))
	{
        if (proc == NULL)
        {
            detectError("readdir in main failed");
            return 0;
        }
		if (charIsNumber(proc->d_name[0]) == 1)
		{
            getInformationAboutProcess(proc);
            numberOfProc++;
		}
	}
	if (closedir(dir) == -1)
	{
        detectError("main closedir failed");
        return 0;
	}
    qsort(processes, numberOfProc, sizeof(struct process), compareProcess);
    printf("--------------------------------------------------------\n");
    for (int i = 0; i < 10; ++i)
    {
        printProc(i + 1, processes[i]);
    }
    printf("--------------------------------------------------------\n");
	return 0;
}
