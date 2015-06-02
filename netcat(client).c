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
#include <semaphore.h>

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
sem_t sem;

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
    {
        sem_post(&sem);
        return NULL;
    }
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
    sem_post(&sem);
    printf("posted!\n");
    return NULL;
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
                pthread_detach(partSender[i]);
                printf("before wait\n");
                sem_wait(&sem);
                printf("after wait\n");
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
    sem_init(&sem, 0, 10);
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
    sem_destroy(&sem);
    close(fd);
    return 0;
}



/*#include <sys/types.h>
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
int sock;
int fileSend;
int initFileSize;
void * addr;
int * readyParts;
int numberOfReadyParts, numberOfParts;

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
    while (numberOfReadyParts < numberOfParts)
    {
        //printf("numberOfParts = %d\n", numberOfReadyParts);
        pthread_t partSender[numberOfParts];
        for (int i = 0; i < numberOfParts; i++)
        {
            if (readyParts[i] == 0)
            {
                struct writerArg arg;
                arg.numberOfPart = i;
                arg.startOfPart = SIZE_OF_PART * i;
                arg.sizeOfPart = SIZE_OF_PART;
                if (i == numberOfParts - 1)
                    arg.sizeOfPart = sizeOfBuf - SIZE_OF_PART * (numberOfParts - 1);
                printf("size of part #%d = %d\n", i, arg.sizeOfPart);
                if(pthread_create(&(partSender[i]), NULL, partWriter, (void *)(&arg)))
                {
                    perror("pthread_create with partSender failed");
                    return 0;
                }
                if (numberOfReadyParts == numberOfParts)
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
            numberOfParts = fileSize / SIZE_OF_PART;
            if (numberOfParts == 0 && fileSize != 0)
                numberOfParts = 1;
            initFileSize = 1;
            readyParts = malloc(numberOfParts * sizeof(int));
            for (int i = 0; i < numberOfParts; i++)
                readyParts[i] = 0;
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
    return NULL;
}

int main(int argc, char ** argv)
{
    int numberOfReadyParts = 0;
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
    while (initFileSize == 0 || numberOfReadyParts < numberOfParts)
    {
        sleep(1);
    }
    free(readyParts);
    close(sock);
    return 0;
}
*/
