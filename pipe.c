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

#define SEMAPHORE_NAME "sem"
#define SHARED_MEMORY_NAME "PartOfSharedMemory"
#define STRING_TO_WRITE "hello world!"

const size_t SHM_SIZE = 3000;
const size_t NUMBER_OF_PIPES;
int fdescr;
extern int errno;
int number_of_pipes = 1;
void * addr;

struct shared_memory
{
    sem_t * semWrite, * semRead;
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
    addr = mmap(NULL, sizeof(struct shared_memory) + SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fdescr, 0);
    if (addr == MAP_FAILED)
    {
        detectError("mmap faild");
        return;
    }
    (*fd)->semWrite = (sem_t *)(addr);
    (*fd)->semRead = (sem_t *)(addr + sizeof(sem_t));
    (*fd)->begin = (int *)(addr + 2 * sizeof(sem_t));
    (*fd)->end = (int *)(addr + sizeof(int) + 2 * sizeof(sem_t));
    (*fd)->fdescr = (int *)(addr + 2 * sizeof(int) + 2 * sizeof(sem_t));
    (*fd)->number = (int *)(addr + 3 * sizeof(int) + 2 * sizeof(sem_t));
    (*fd)->addr = (void *)(addr + 4 * sizeof(int) + 2 * sizeof(sem_t));
    (*fd)->buf = (char *)(addr + 10 * sizeof(int) + 2 * sizeof(sem_t));
    //char sem_name[26] = SEMAPHORE_NAME;
    //sprintf(sem_name + strlen(SEMAPHORE_NAME), "%d", number_of_pipes);
    //printf("1\n");
    sem_init((*fd)->semWrite, 1, 1);
    sem_init((*fd)->semRead, 1, 1);
    //printf("2\n");
    if ((*fd)->semWrite == SEM_FAILED)
    {
        detectError("sem_open failed");
        return;
    }
    if ((*fd)->semRead == SEM_FAILED)
    {
        detectError("sem_open failed");
        return;
    }
    sem_post((*fd)->semWrite);
    sem_post((*fd)->semRead);
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
    sem_destroy(fd->semWrite);
    sem_destroy(fd->semRead);
}

size_t sizeOfPipe(struct shared_memory * fd)
{
    int result = *(fd->end) - *(fd->begin);
    if (result < 0)
    {
        result += SHM_SIZE;
    }
    return result;
}

size_t writeToPipe(struct shared_memory * fd, char * buf, size_t size_of_buf)
{
    if (sem_wait(fd->semWrite) == -1)
    {
        detectError("sem_wait in writeToPipe failed");
        return;
    }
    size_t tab;
    for (tab = 0; tab < size_of_buf && sizeOfPipe(fd) != SHM_SIZE; tab++)
    {
        if (*(fd->end) == SHM_SIZE)
        {
            *(fd->end) = 0;
        }
        (fd->buf)[*(fd->end)] = buf[tab];
        *(fd->end) += 1;
    }
    if (sem_post(fd->semWrite) == -1)
    {
        detectError("sem_post in writeToPipe failed");
        return;
    }
    return tab;
}

int readFromPipe(struct shared_memory * fd, char * buf, size_t size_of_part_to_read)
{
    if (buf == NULL)
    {
        detectError("malloc in readFromPipe failed");
        return -1;
    }
    //printf("reading is coming...\n");
    if (sem_wait(fd->semWrite) == -1)
    {
        detectError("sem_wait in readFromPipe failed");
        return -1;
    }
    int tab = 0;
    if (size_of_part_to_read == 0)
        return 0;
    //printf("reading begin...\n");
    while (sizeOfPipe(fd) > 0 && tab < size_of_part_to_read)
    {
        //printf("begin = %d, end = %d\n", *(fd->begin), *(fd->end));
        if (*(fd->begin) == SHM_SIZE)
            *(fd->begin) = 0;
        buf[tab] = (fd->buf)[*(fd->begin)];
        //printf("%d = %c\n", *(fd->begin), (fd->buf)[*(fd->begin)]);
        //fflush(stdout);
        *(fd->begin) += 1;
        tab++;
    }
    //printf("reading end!\n");
    if (sem_post(fd->semWrite) == -1)
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
    int total = 1000 * strlen(STRING_TO_WRITE), already_done = 0;
    pid_t t = fork();
    int alreadyWrite = 0;
    if (t == 0)
    {
        for (int i = 0; i < 1000; i++)
        {
            while (alreadyWrite < total)
            {
                alreadyWrite += writeToPipe(fd, STRING_TO_WRITE, strlen(STRING_TO_WRITE));
                //sprintf("alreadyWrite = %d\n", alreadyWrite);
            }
        }
    }
    else
    {
        sleep(1);
        //printf("%d\n", total);
        char * buf = malloc(total * sizeof(char));
        if (buf == NULL)
        {
            detectError("mallok in child process failed");
            return 0;
        }
        //printf("wait...\n");
        while (already_done < total)
        {
            //printf("wait...\n");
            already_done += readFromPipe(fd, buf + already_done, total - already_done);
            //printf("already_done = %d\n", already_done);
        }
        //sleep(1);
        buf[total] = '\0';
        //write(STDOUT_FILENO, buf, total);
        //printf("\n");
        printf("buf = %s\n", buf);
        free(buf);
        deletePipe(fd);
        return 0;
    }
    return 0;
}
