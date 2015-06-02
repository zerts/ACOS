#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#define MAX_NAME_SIZE 30

#define BUF_SIZE 1024
#define NUMBER_OF_PARTS 10
#define SIZE_OF_PART 100

void detectError(char * err)
{
    perror(err);
}

pthread_mutex_t forRead, forWrite;
char * fileName;
int numberOfRooms, currRoom;
int fd, addrSize;
socklen_t addrLen;
int fileSend, initFileSize;
void * addr;
int * readyParts;
int numberOfReadyParts, numberOfParts;

struct writerArg
{
    int numberOfPart;
    int startOfPart;
    int sizeOfPart;
    struct sockaddr_in servAddr;
};

struct stableSendArg
{
    struct sockaddr_in servAddr;
    int fileSize;
};

int listenTheMessage(struct sockaddr_in servAddr, int flag)
{
    int message = -1;
    int curr = 0;
    pthread_mutex_lock(&forRead);
    while (curr < sizeof(int))
    {
        int nextPart = recvfrom(fd, (char*)(&message) + curr, sizeof(int) - curr, flag, (struct sockaddr *)&servAddr, &addrLen);
        if (nextPart < 0)
        {
            detectError("recv failed");
            exit(0);
        }
        curr += nextPart;
    }
    //printf("message = %d\n", message);
    fflush(stdout);
    pthread_mutex_unlock(&forRead);
    return message;
}

void * partWriter(void * arg)
{
    ///bad connection!!!
    if(rand() % 2)
        return NULL;
    int numberOfPart = (*(struct writerArg *)arg).numberOfPart;
    int startOfPart = (*(struct writerArg *)arg).startOfPart;
    int sizeOfPart = (*(struct writerArg *)arg).sizeOfPart;
    struct sockaddr_in servAddr = (*(struct writerArg *)arg).servAddr;
    int alreadySend = 0, nextPart;
    pthread_mutex_lock(&forWrite);
    while (sizeof(int) > alreadySend)
    {
        nextPart = sendto(fd, &numberOfPart + alreadySend, sizeof(int) - alreadySend, 0, (struct sockaddr *)&servAddr, addrSize);
        alreadySend += nextPart;
    }
    alreadySend = 0;
    while (sizeOfPart > alreadySend)
    {
        nextPart = sendto(fd, addr + startOfPart + alreadySend, sizeOfPart - alreadySend, 0, (struct sockaddr *)&servAddr, addrSize);
        alreadySend += nextPart;
    }
    pthread_mutex_unlock(&forWrite);
}

void * stableSendMessage(void * arg)
{
    int fileSize = (*(struct stableSendArg *)arg).fileSize;
    struct sockaddr_in servAddr = (*(struct stableSendArg *)arg).servAddr;
    pthread_mutex_lock(&forWrite);
    int alreadySend = 0, nextPart;
    while (sizeof(int) > alreadySend)
    {
        nextPart = sendto(fd, &fileSize + alreadySend, sizeof(int) - alreadySend, 0, (struct sockaddr *)&servAddr, addrSize);
        alreadySend += nextPart;
    }
    printf("file size sent! %d\n", fileSize);
    pthread_mutex_unlock(&forWrite);
    alreadySend = 0;
    while (numberOfReadyParts < numberOfParts)
    {
        pthread_t partSender[numberOfParts];
        for (int i = 0; i < numberOfParts; i++)
        {
            if (readyParts[i] == 0)
            {
                struct writerArg arg;
                arg.numberOfPart = i;
                arg.startOfPart = SIZE_OF_PART * i;
                arg.sizeOfPart = SIZE_OF_PART;
                arg.servAddr = servAddr;

                if (i == numberOfParts - 1)
                    arg.sizeOfPart = fileSize - SIZE_OF_PART * (numberOfParts - 1);

                if(pthread_create(&(partSender[i]), NULL, partWriter, (void *)(&arg)))
                {
                    perror("pthread_create with partSender failed");
                    return NULL;
                }
                if (numberOfReadyParts == numberOfParts)
                    break;
            }
        }
    }
    printf("ready!\n");
    exit(0);
}

void sending(struct sockaddr_in servAddr)
{
    while (1)
    {
        int retVal = listenTheMessage(servAddr, 0);
        if (retVal == -2 && fileSend == 0)
        {
            fileSend = 1;
            char line[10];
            int file = open(fileName, O_CREAT | O_RDWR, 0777);

            struct stat sizeOfFile;
            stat(fileName, &sizeOfFile);
            int fileSize = (int)sizeOfFile.st_size;
            numberOfParts = fileSize / SIZE_OF_PART;

            if (numberOfParts == 0 && fileSize != 0)
                numberOfParts = 1;
            initFileSize = 1;
            readyParts = malloc(numberOfParts * sizeof(int));
            for (int i = 0; i < numberOfParts; i++)
                readyParts[i] = 0;

            addr = mmap(NULL, fileSize, PROT_WRITE | PROT_READ, MAP_SHARED, file, 0);

            pthread_t totalSender;
            struct stableSendArg arg;
            arg.servAddr = servAddr;
            arg.fileSize = fileSize;
            printf("send the file size!\n");
            if(pthread_create(&totalSender, NULL, stableSendMessage, (void *)(&arg)))
            {
                perror("pthread_create failed");
                return;
            }
        }
        else if (retVal == -3)
        {
            close(fd);
            exit(0);
        }
        else if (retVal >= 0 && retVal <numberOfParts)
        {
            if (readyParts[retVal] == 0)
            {
                printf("part #%d is done!\n", retVal);
                fflush(stdout);
                readyParts[retVal] = 1;
                numberOfReadyParts++;
            }
        }
    }
    return;
}

int main(int argc, char ** argv)
{
    int numberOfReadyParts = 0;
    fileSend = 0;
    struct sockaddr_in servAddr;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        detectError("socket failed");
        return 0;
    }
    fileName = argv[3];
    int filenameLength = strlen(fileName);

    memset((char *)&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(atoi(argv[2]));
    servAddr.sin_addr.s_addr = inet_addr(argv[1]);
    addrSize = sizeof servAddr;
    addrLen = sizeof(servAddr);

    int signalOfReady = 123;
    sendto(fd, &signalOfReady, sizeof(int), 0, (struct sockaddr *)&servAddr, addrSize);
    sendto(fd, &filenameLength, sizeof(int), 0, (struct sockaddr *)&servAddr, addrSize);
    printf("send the filename size\n");
    sendto(fd, fileName, strlen(fileName), 0, (struct sockaddr *)&servAddr, addrSize);
    sending(servAddr); 
    free(readyParts);
    close(fd);
    return 0;
}

