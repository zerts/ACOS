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
#define NUMBER_OF_PARTS 10

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
    int sock;
    char * fileName;
};

struct queque
{
    int begin, end;
    struct client * clients;
};

void que_init(struct queque * que)
{
    que->clients = malloc(MAX_NUMBER_OF_CLIENTS * sizeof(struct client));
    que->begin = 0;
    que->end = 0;
}

struct client arg;
int activeUsers;
struct queque clients;
pthread_mutex_t writeMutex, readMutex;

struct client * readMessageFrom(int sock)
{
    int lengthOfMessage;
    //printf("reading...\n");
    recv(sock, &lengthOfMessage, sizeof(int), 0);
	struct client * message = malloc(sizeof(struct client));
	char * currMessage = (char *) malloc(lengthOfMessage + 1);
	currMessage[0] = '\0';
	int alreadyRead = 0, nextPart;
	while (alreadyRead < lengthOfMessage)
	{
        nextPart = recv(sock, currMessage + alreadyRead, lengthOfMessage - alreadyRead, 0);
        alreadyRead += nextPart;
	}
	message->fileName = currMessage;
	//printf("%d\n", lengthOfMessage);
	//printf("%s\n", currMessage);
    return message;
}

void readFromFile(int sock, char * fileName)
{
    char * parts[NUMBER_OF_PARTS];
    int numberOfReadyPart = 0;
    int readyParts[NUMBER_OF_PARTS];
    for (int i = 0; i < NUMBER_OF_PARTS; i++)
        readyParts[i] = 0;

    int lengthOfFile;
    //printf("reading...\n");
    //printf("i'm ready for reading a file!\n");
    recv(sock, &lengthOfFile, sizeof(int), 0);
    int sizeOfParts[NUMBER_OF_PARTS];
    for (int i = 0; i < NUMBER_OF_PARTS; i++)
    {
        if (i != NUMBER_OF_PARTS - 1)
            sizeOfParts[i] = lengthOfFile / NUMBER_OF_PARTS;
        else
            sizeOfParts[i] = lengthOfFile - (lengthOfFile / NUMBER_OF_PARTS) * (NUMBER_OF_PARTS - 1);
        parts[i] = malloc(sizeOfParts[i] * sizeof(char));
        //printf("size of part #%d = %d\n", i, sizeOfParts[i]);
    }
	//char * currFile = (char *) malloc(lengthOfFile + 1);
	//currFile[0] = '\0';
	printf("i'm ready for reading parts!\n");
	while (numberOfReadyPart < NUMBER_OF_PARTS)
	{
        ///bad connection!!!
        if (rand() % 2)
            continue;
        pthread_mutex_lock(&readMutex);
        int numberOfPart;
        recv(sock, &numberOfPart, sizeof(int), 0);
        //printf("i'm start to reading part #%d!\n", numberOfPart);
        int alreadyRead = 0, nextPart;
        while (alreadyRead < sizeOfParts[numberOfPart])
        {
            nextPart = recv(sock, parts[numberOfPart] + alreadyRead, sizeOfParts[numberOfPart] - alreadyRead, 0);
            alreadyRead += nextPart;
        }
        //printf("i've read part #%d\n", numberOfPart);
        if (readyParts[numberOfPart] == 0)
        {
            readyParts[numberOfPart]++;
            numberOfReadyPart++;
        }
        send(sock, &numberOfPart, sizeof(int), 0);
        pthread_mutex_unlock(&readMutex);
        //printf("i've finish part #%d\n", numberOfPart);
    }
	int alreadyCopy = 0;
	printf("i'm ready for make a full file!\n");
	int fd = open(fileName, O_CREAT | O_RDWR, 0777);
	int alreadyWrite = 0, nextPart;
	for (int i = 0; i < NUMBER_OF_PARTS; i++)
	{
        alreadyWrite = 0;
        while (alreadyWrite < sizeOfParts[i])
        {
            nextPart = write(fd, parts[i] + alreadyWrite, sizeOfParts[i] - alreadyWrite);
            alreadyWrite += nextPart;
        }
        //sprintf(currFile + alreadyCopy, "%s", parts[i]);
        alreadyCopy += sizeOfParts[i];
        free(parts[i]);
	}
	printf("i've write a file!\n");
	//free(currFile);
	close(fd);
}

void * workWithClient (void * _arg)
{
    char * fileName = (*(struct client *)_arg).fileName;
    int sockOfClient = (*(struct client *)_arg).sock;
    //printf("fileName = %s\n", fileName);
    pthread_mutex_lock(&writeMutex);
    int readyCode = -2;
    int finishCode = -3;
    for (int i = 0; i < 100; i++)
        send(sockOfClient, &readyCode, sizeof(int), 0);
    readFromFile(sockOfClient, fileName);
    for (int i = 100; i < 200; i++)
        send(sockOfClient, &i, sizeof(int), 0);
    pthread_mutex_unlock(&writeMutex);
}

void getFile(struct client currClient)
{
    arg.sock = currClient.sock;
    arg.fileName = currClient.fileName;
    pthread_t sender;
    pthread_create(&sender, NULL, workWithClient, (void *)&arg);
}

int main(int argc, char ** argv)
{
    activeUsers = 0;
    if (argc <= 2)
    {
        fprintf(stderr, "Usage host port\n");
        return 0;
    }
    int sockDescr = socket(AF_INET, SOCK_STREAM, 0);
    if (sockDescr < 0)
    {
        detectError("socket failed");
        return 0;
    }
    que_init(&clients);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    char * path = argv[3];
    addr.sin_port = htons(atoi(argv[2]));
    addr.sin_addr.s_addr = inet_addr(argv[1]);
    for(int i = 0; i < MAX_NUMBER_OF_CLIENTS; i++)
    {
        clients.clients[i].sock = -1;
    }
    if(bind(sockDescr, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        detectError("bind failed");
        return 0;
    }

    listen(sockDescr, MAX_NUMBER_OF_QUERIES);

    struct epoll_event ev, events[MAX_NUMBER_OF_CLIENTS];
    int epollFD = epoll_create(MAX_NUMBER_OF_CLIENTS);
    if (epollFD == -1)
    {
        detectError("epoll_create failed");
        return 0;
    }

    ev.events = EPOLLIN;
    ev.data.fd = sockDescr;
    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, sockDescr, &ev) == -1)
    {
        detectError("epoll_ctl failed");
        return 0;
    }

    while (1)
    {
        int ndfs = epoll_wait(epollFD, events, MAX_NUMBER_OF_CLIENTS, -1);
        if (ndfs == -1)
        {
            detectError("epoll_wait failed");
            printf("vsyo3!\n");
            return 0;
        }

        for (int i = 0; i < ndfs; ++i)
        {
            int sock = events[i].data.fd;
            if (events[i].data.fd == sockDescr)
            {
                printf("new man\n");
                sock = accept(sockDescr, NULL, NULL);
                if(sock < 0)
                {
                    detectError("accept failed");
                    printf("vsyo2!\n");
                    return 0;
                }
                clients.clients[clients.end].fileName = malloc(strlen(path) + 30);
                char * fileNameFromUser = (readMessageFrom(sock))->fileName;
                sprintf(clients.clients[clients.end].fileName, "%s", path);
                sprintf(clients.clients[clients.end].fileName + strlen(path), "%s", fileNameFromUser);
                fprintf(stderr, "%s put in queque\n", clients.clients[clients.end].fileName);
                clients.clients[clients.end].sock = sock;
                activeUsers++;
                clients.end = (clients.end + 1) % MAX_NUMBER_OF_CLIENTS;
                if (clients.end == clients.begin)
                {
                    printf("to many clients!\n");
                    sleep(1);
                }
                ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
                ev.data.fd = sock;
                if (epoll_ctl(epollFD, EPOLL_CTL_ADD, sock, &ev) == -1)
                {
                    detectError("epoll_ctl: conn_sock");
                    printf("vsyo1!\n");
                    return 0;
                }
            }
            else
            {
               if (events[i].events & EPOLLRDHUP)
               {
                    for(int j = 0; j < MAX_NUMBER_OF_CLIENTS; j++)
                    {
                        if(clients.clients[j].sock == sock)
                        {
                            clients.clients[j].sock = -1;
                            activeUsers--;
                            fprintf(stderr, "%d done\n", j);
                            break;
                        }
                    }
                }
            }
        }
        if (clients.begin != clients.end)
        {
            getFile(clients.clients[clients.begin]);
            printf("client #%d done!\n", clients.begin);
            clients.begin = (clients.begin + 1) % MAX_NUMBER_OF_CLIENTS;
        }
    }
    printf("vsyo!\n");
    return 0;
}
