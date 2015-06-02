#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define MAX_NUMBER_OF_CLIENTS 128
#define MAX_NUMBER_OF_QUERIES 20
#define SIZE_OF_PART 100

extern int errno;

void detectError(char * err)
{
    perror(err);
    errno = 0;
}

struct mess
{
    int clientNumber;
    char * message;
};

struct client
{
    struct sockaddr_in addr;
    char * fileName;
};

char * path;
int addrsize;
socklen_t addrlen;
struct client arg;
int activeUsers;
int numberOfParts;
int ready, fd;
pthread_mutex_t writeMutex, readMutex;

void readFromFile(struct sockaddr_in claddr, char * fileName)
{
    int lengthOfFile;
    //printf("reading...\n");
    //printf("i'm ready for reading a file!\n");
    recvfrom(fd, &lengthOfFile, sizeof(int), 0, (struct sockaddr *)&claddr, &addrlen);
    printf("size of file = %d\n", lengthOfFile);
    numberOfParts = lengthOfFile / SIZE_OF_PART;
    if (numberOfParts == 0 && lengthOfFile != 0)
        numberOfParts = 1;

    int sizeOfParts[numberOfParts];
    char * parts[numberOfParts];
    int numberOfReadyPart = 0;
    int readyParts[numberOfParts];

    for (int i = 0; i < numberOfParts; i++)
        readyParts[i] = 0;
    for (int i = 0; i < numberOfParts; i++)
    {
        if (i != numberOfParts - 1)
            sizeOfParts[i] = SIZE_OF_PART;
        else
            sizeOfParts[i] = lengthOfFile - SIZE_OF_PART * (numberOfParts - 1);
        parts[i] = malloc(sizeOfParts[i] * sizeof(char));
        //printf("size of part #%d = %d\n", i, sizeOfParts[i]);
    }

	//printf("i'm ready for reading parts! In number of %d\n", numberOfParts);
	while (numberOfReadyPart < numberOfParts)
	{
        ///bad connection!!!
        if (rand() % 2)
            continue;
        pthread_mutex_lock(&readMutex);
        int numberOfPart;
        recvfrom(fd, &numberOfPart, sizeof(int), 0, (struct sockaddr *)&claddr, &addrlen);
        //printf("i'm start to reading part #%d!\n", numberOfPart);
        int alreadyRead = 0, nextPart;
        while (alreadyRead < sizeOfParts[numberOfPart])
        {
            nextPart = recvfrom(fd, parts[numberOfPart] + alreadyRead, sizeOfParts[numberOfPart] - alreadyRead, 0,
                                (struct sockaddr *)&claddr, &addrlen);
            alreadyRead += nextPart;
        }
        //printf("i've read part #%d\n", numberOfPart);
        if (readyParts[numberOfPart] == 0)
        {
            readyParts[numberOfPart]++;
            numberOfReadyPart++;
        }
        sendto(fd, &numberOfPart, sizeof(int), 0, (struct sockaddr *)&claddr, addrsize);
        pthread_mutex_unlock(&readMutex);
        //printf("i've finish part #%d\n", numberOfPart);
    }
	int alreadyCopy = 0;
    char * pathOfFile = malloc(strlen(path) + strlen(fileName) + 1);
    sprintf(pathOfFile, "%s%s", path, fileName);
	printf("i'm ready for make a full file into %s\n", pathOfFile);
	int fileDescr = open(pathOfFile, O_CREAT | O_RDWR, 0777);
	int alreadyWrite = 0, nextPart;
	for (int i = 0; i < numberOfParts; i++)
	{
        //printf("i've started to write a part #%d!\n", i);
        alreadyWrite = 0;
        while (alreadyWrite < sizeOfParts[i])
        {
            //printf("alreadyWrite = %d\n", alreadyWrite);
            nextPart = write(fileDescr, parts[i] + alreadyWrite, sizeOfParts[i] - alreadyWrite);
            alreadyWrite += nextPart;
        }
        alreadyCopy += sizeOfParts[i];
        free(parts[i]);
	}
    free(pathOfFile);
	printf("i've write a file!\n");
	close(fileDescr);
}

void * worker (void * _arg)
{
    int filenameLength;
    struct sockaddr_in claddr = *(struct sockaddr_in *)_arg;

    recvfrom(fd, &filenameLength, sizeof(int), 0, (struct sockaddr *)&claddr, &addrlen);
    char * fileName = malloc(filenameLength * sizeof(char));
    recvfrom(fd, fileName, filenameLength, 0, (struct sockaddr *)&claddr, &addrlen);
    printf("fileName = %s\n", fileName);

    pthread_mutex_lock(&writeMutex);
    int readyCode = -2;
    int finishCode = -3;
    for (int i = 0; i < 100; i++)
        sendto(fd, &readyCode, sizeof(int), 0, (struct sockaddr *)&claddr, addrsize);
    readFromFile(claddr, fileName);
    for (int i = 100; i < 200; i++)
        sendto(fd, &i, sizeof(int), 0, (struct sockaddr *)&claddr, addrsize);
    pthread_mutex_unlock(&writeMutex);
    return NULL;
}

int workWithClient (struct sockaddr_in claddr)
{
    ready = 0;

    pthread_t newWorker;
    pthread_create(&newWorker, NULL, worker, (void *)&claddr);
    pthread_join(newWorker, NULL);
   /* while (1)
        if (ready != 0)
            break;*/
    return 1;
}

int main(int argc, char ** argv)
{
    activeUsers = 0;
    if (argc <= 2)
    {
        fprintf(stderr, "Usage host port\n");
        return 0;
    }
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        detectError("socket failed");
        return 0;
    }
    struct sockaddr_in myaddr;
    myaddr.sin_family = AF_INET;
    path = argv[3];
    myaddr.sin_port = htons(atoi(argv[2]));
    myaddr.sin_addr.s_addr = inet_addr(argv[1]);
    addrlen = sizeof(myaddr);
    addrsize = sizeof myaddr;
    if(bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0)
    {
        detectError("bind failed");
        return 0;
    }

    while (1)
    {
        struct sockaddr_in claddr;
        myaddr.sin_family = AF_INET;
        myaddr.sin_port = htons(atoi(argv[2]));
        myaddr.sin_addr.s_addr = INADDR_ANY;
        int signalOfReady;
        recvfrom(fd, &signalOfReady, sizeof(int), 0, (struct sockaddr *)&claddr, &addrlen);
        if (signalOfReady == 123)
            while (workWithClient(claddr) != 1);
    }
    printf("vsyo!\n");
    return 0;
}
