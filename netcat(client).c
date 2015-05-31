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

void detectError(char * err)
{
    perror(err);
}

pthread_mutex_t forRead, forWrite;
char * fileName;
int numberOfRooms, currRoom;
int sock;
int fileSend;
void * addr;
int readyParts[NUMBER_OF_PARTS];
int numberOfReadyParts;

struct writerArg
{
    int numberOfPart;
    int startOfPart;
    int sizeOfPart;
};

struct stableSendArg
{
    int sock;
    int fileSize;
};

void sendMessage(int sock, char * buf, int sizeOfBuf, int flag)
{
    pthread_mutex_lock(&forWrite);
    int alreadySend = 0, nextPart;
    while (sizeof(int) > alreadySend)
    {
        nextPart = send(sock, &sizeOfBuf + alreadySend, sizeof(int) - alreadySend, flag);
        alreadySend += nextPart;
    }
    alreadySend = 0;
    while (sizeOfBuf > alreadySend)
    {
        nextPart = send(sock, buf + alreadySend, sizeOfBuf - alreadySend, flag);
        alreadySend += nextPart;
    }
    pthread_mutex_unlock(&forWrite);
}

int listenTheMessage(int currSock, int flag)
{
    int message = -1;
    int curr = 0;
    pthread_mutex_lock(&forRead);
    while (curr < sizeof(int))
    {
        int nextPart = recv(currSock, (char*)(&message) + curr, sizeof(int) - curr, flag);
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
        return;
    int numberOfPart = (*(struct writerArg *)arg).numberOfPart;
    int startOfPart = (*(struct writerArg *)arg).startOfPart;
    int sizeOfPart = (*(struct writerArg *)arg).sizeOfPart;
    int alreadySend = 0, nextPart;
    pthread_mutex_lock(&forWrite);
    //printf("I start to send part #%d\n", numberOfPart);
    //fflush(stdout);
    while (sizeof(int) > alreadySend)
    {
        nextPart = send(sock, &numberOfPart + alreadySend, sizeof(int) - alreadySend, 0);
        alreadySend += nextPart;
    }
    alreadySend = 0;
    //printf("part #%d\n", numberOfPart);
    while (sizeOfPart > alreadySend)
    {
        nextPart = send(sock, addr + startOfPart + alreadySend, sizeOfPart - alreadySend, 0);
        //write(STDOUT_FILENO, addr + startOfPart + alreadySend, sizeOfPart - alreadySend);
        alreadySend += nextPart;
    }
    pthread_mutex_unlock(&forWrite);
}

void * stableSendMessage(void * arg)
{
    int sizeOfBuf = (*(struct stableSendArg *)arg).fileSize;
    int sock = (*(struct stableSendArg *)arg).sock;
    pthread_mutex_lock(&forWrite);
    int alreadySend = 0, nextPart;
    while (sizeof(int) > alreadySend)
    {
        nextPart = send(sock, &sizeOfBuf + alreadySend, sizeof(int) - alreadySend, 0);
        alreadySend += nextPart;
    }
    pthread_mutex_unlock(&forWrite);
    alreadySend = 0;
    while (numberOfReadyParts < NUMBER_OF_PARTS)
    {
        printf("numberOfParts = %d\n", numberOfReadyParts);
        pthread_t partSender[NUMBER_OF_PARTS];
        for (int i = 0; i < NUMBER_OF_PARTS; i++)
        {
            if (readyParts[i] == 0)
            {
                struct writerArg arg;
                arg.numberOfPart = i;
                arg.startOfPart = (sizeOfBuf / NUMBER_OF_PARTS) * i;
                arg.sizeOfPart = sizeOfBuf / NUMBER_OF_PARTS;
                if (i == NUMBER_OF_PARTS - 1)
                    arg.sizeOfPart = sizeOfBuf - (sizeOfBuf / NUMBER_OF_PARTS) * i;
                //printf("size of part #%d = %d\n", i, arg.sizeOfPart);
                if(pthread_create(&(partSender[i]), NULL, partWriter, (void *)(&arg)))
                {
                    perror("pthread_create with partSender failed");
                    return 0;
                }
                if (numberOfReadyParts == NUMBER_OF_PARTS)
                    break;
            }
        }
        //sleep(1);
    }
    printf("ready!\n");
    exit(0);
}

void * listenAll(void * args)
{
    int currSock = *(int *)(args);
    while (1)
    {
        int retVal = listenTheMessage(currSock, 0);
        if (retVal == -2 && fileSend == 0)
        {
            fileSend = 1;
            char line[10];
            int fd = open(fileName, O_CREAT | O_RDWR, 0777);
            struct stat sizeOfFile;
            stat(fileName, &sizeOfFile);
            int fileSize = (int)sizeOfFile.st_size;
            addr = mmap(NULL, fileSize, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
            pthread_t totalSender;
            struct stableSendArg arg;
            arg.sock = currSock;
            arg.fileSize = fileSize;
            if(pthread_create(&totalSender, NULL, stableSendMessage, (void *)(&arg)))
            {
                perror("pthread_create failed");
                return 0;
            }
        }
        else if (retVal == -3)
        {
            close(currSock);
            exit(0);
        }
        else if (retVal >= 0 && retVal <NUMBER_OF_PARTS)
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
    return NULL;
}

int main(int argc, char ** argv)
{
    int numberOfReadyParts = 0;
    for (int i = 0; i < NUMBER_OF_PARTS; i++)
        readyParts[i] = 0;
    fileSend = 0;
    struct sockaddr_in sockAddr;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        detectError("socket failed");
        return 0;
    }
    sockAddr.sin_family = AF_INET;
    fileName = argv[3];
    sockAddr.sin_port = htons(atoi(argv[2]));
    sockAddr.sin_addr.s_addr = inet_addr(argv[1]);
    if(connect(sock, (struct sockaddr *)&sockAddr, sizeof(sockAddr)) < 0)
    {
        detectError("connect failed");
        return 0;
    }
    sendMessage(sock, fileName, strlen(fileName), 0);
    pthread_t listener;
    if(pthread_create(&listener, NULL, listenAll, (void *)(&sock)))
    {
    	perror("pthread_create failed");
    	return 0;
    }
    while (numberOfReadyParts < NUMBER_OF_PARTS)
    {
        sleep(1);
    }
    close(sock);
    return 0;
}
