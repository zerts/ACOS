#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <semaphore.h>

#define SEMAPHORE_NAME "sem__"
#define SHARED_MEMORY_NAME "newPartOfSharedMemory"
#define STRING_TO_WRITE "bbb"

const size_t SHM_SIZE = 8192;
const size_t NUMBER_OF_PIPES;
int fdescr;
extern int errno;
int number_of_pipes = 1;

struct shared_memory
{
    sem_t * sem;
    int * begin, * end;
    int * fdescr;
    int * number;
    char * buf;
    void * addr;
};

void detectError(char * err)
{
    perror(err);
    errno = 0;
}

size_t shared_memory_size = sizeof(struct shared_memory);

void create_pipe(struct shared_memory ** fd)
{
    char shm_name[26] = SHARED_MEMORY_NAME;
    sprintf(shm_name + strlen(SHARED_MEMORY_NAME), "%d", number_of_pipes);
    fdescr = shm_open(shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    //printf("shm_open\n");
    if (fdescr == -1)
    {
        detectError("shn_open failed");
        return;
    }
    if (ftruncate(fdescr, shared_memory_size) == -1)
    {
        detectError("ftruncate failed");
        return;
    }
    void * addr = mmap(NULL, shared_memory_size + SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fdescr, 0);
    if (addr == MAP_FAILED)
    {
        detectError("mmap faild");
        return;
    }
    (*fd)->begin = (int *)(addr);
    (*fd)->end = (int *)(addr + sizeof(int));
    (*fd)->fdescr = (int *)(addr + 2 * sizeof(int));
    (*fd)->number = (int *)(addr + 3 * sizeof(int));
    (*fd)->addr = (void *)(addr + 4 * sizeof(int));
    (*fd)->buf = (char *)(addr + 10 * sizeof(int));
    char sem_name[26] = SEMAPHORE_NAME;
    sprintf(sem_name + strlen(SEMAPHORE_NAME), "%d", number_of_pipes);
    (*fd)->sem = sem_open(sem_name, O_CREAT);
    //printf("sem_open\n");
    if ((*fd)->sem == SEM_FAILED)
    {
        detectError("sem_open failed");
        return;
    }
    sem_post((*fd)->sem);
    *((*fd)->begin) = 0;
    *((*fd)->end) = 0;
    *((*fd)->number) = number_of_pipes;
    *((*fd)->fdescr) = fdescr;
    number_of_pipes++;
}

void deletePipe(struct shared_memory * fd)
{
    munmap(fd->addr, shared_memory_size + SHM_SIZE);
    char shm_name[26] = SHARED_MEMORY_NAME;
    sprintf(shm_name + strlen(SHARED_MEMORY_NAME), "%d", *(fd->number));
    shm_unlink(shm_name);
    close(*(fd->fdescr));
    sem_close(fd->sem);
}


void writeToPipe(struct shared_memory * fd, char * buf, size_t size_of_buf)
{
    //printf("write begin!\n");
    //printf("wait in write...\n");
    if (sem_wait(fd->sem) == -1)
    {
        detectError("sem_wait in writeToPipe failed");
        return;
    }
    //printf("go in write!\n");
    for (size_t tab = 0; tab < size_of_buf; tab++)
    {
        if (*(fd->end) > SHM_SIZE - 5)
            *(fd->end) = 0;
        (fd->buf)[*(fd->end)] = buf[tab];
        *(fd->end) += 1;
    }
    if (sem_post(fd->sem) == -1)
    {
        detectError("sem_post in writeToPipe failed");
        return;
    }
    //printf("write done!\n");
}

size_t sizeOfPipe(struct shared_memory * fd)
{
    int result = (int)*(fd->end) - (int)*(fd->begin);
    if (result < 0)
    {
        result += SHM_SIZE;
    }
    return result + 1;
}

int readFromPipe(struct shared_memory * fd, char * buf, size_t size_of_part_to_read)
{
    if (buf == NULL)
    {
        detectError("malloc in readFromPipe failed");
        return -1;
    }
    //printf("wait...\n");
    if (sem_wait(fd->sem) == -1)
    {
        detectError("sem_wait in readFromPipe failed");
        return -1;
    }
    //printf("go!\n");
    int tab = 0;
    if (size_of_part_to_read == 0)
        return 0;
    while (*(fd->begin) != *(fd->end) && tab < size_of_part_to_read && sizeOfPipe(fd) > 0)
    {
        //printf("while in read\n");
        if (*(fd->begin) == SHM_SIZE)
            *(fd->begin) = 0;
        buf[tab] = (fd->buf)[*(fd->begin)];
        *(fd->begin) += 1;
        tab++;
    }
    if (sem_post(fd->sem) == -1)
    {
        detectError("sem_post in readFromPipe failed");
        return -1;
    }
    return tab;
}

int main()
{
    struct shared_memory * fd = malloc(sizeof(struct shared_memory));
    create_pipe(&fd);
    char a = '1';
    pid_t t = fork();
    if (t == 0)
    {
        writeToPipe(fd, STRING_TO_WRITE, strlen(STRING_TO_WRITE));
    }
    else
    {
    //sleep(1);
        int total = strlen(STRING_TO_WRITE), already_done = 0;
        char * buf = malloc(total);
        if (buf == NULL)
        {
            detectError("mallok in child process failed");
            return 0;
        }
        while(1)
        {
            while (already_done < total)
            {
                //printf("while\n");
                already_done += readFromPipe(fd, buf + already_done, total - already_done);
            }
            //printf("%d\n", total);
            buf[total] = '\0';
            printf("buf = %s\n", buf);
            //sleep(1);
        }
        deletePipe(fd);
        return 0;
    }
    return 0;
}
