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

#define MAX_NUMBER_OF_CLIENTS 128
#define MAX_NUMBER_OF_QUERIES 20
#define MAX_NUMBER_OF_ROOMS 1024

extern int errno;

void detectError(char * err)
{
    perror(err);
    errno = 0;
}

struct client
{
    char * name;
    int sock;
};

struct mess
{
    int clientNumber;
    int room;
    char * message;
};

struct mess arg;
int activeUsers;
int rooms[MAX_NUMBER_OF_ROOMS];
struct client clients[MAX_NUMBER_OF_CLIENTS];
pthread_mutex_t writeMutex;

struct mess * readMessageFrom(int sock)
{
    int lengthOfMessage, roomOfSender;
    printf("reading...\n");
    recv(sock, &lengthOfMessage, sizeof(int), 0);
    recv(sock, &roomOfSender, sizeof(int), 0);
	struct mess * message = malloc(sizeof(struct mess));
	message->room = roomOfSender;
	char * currMessage = (char *) malloc(lengthOfMessage + 1);
	currMessage[0] = '\0';
	int alreadyRead = 0, nextPart;
	while (alreadyRead < lengthOfMessage)
	{
        nextPart = recv(sock, currMessage + alreadyRead, lengthOfMessage - alreadyRead, 0);
        alreadyRead += nextPart;
	}
	message->message = currMessage;
	printf("%d\n", lengthOfMessage);
	printf("%s\n", currMessage);
	if (currMessage[0] == '-')
    {
        printf("putting is in process...\n");
        rooms[lengthOfMessage] = roomOfSender;
        return NULL;
    }
    return message;
}

void * sendMessage (void * _arg)
{
    char * message = (*(struct mess *)_arg).message;
    int lengthOfMessage = strlen(message);
    int numberOfRoom = (*(struct mess *)_arg).room;
    int numberOfSender = (*(struct mess *)_arg).clientNumber;
    char * nameOfSender = clients[numberOfSender].name;
    int lengthOfName = strlen(nameOfSender);
    int numberOfRooms = 1;
    int currRoom = rooms[numberOfRoom];
    while (currRoom != -1)
    {
        currRoom = rooms[currRoom];
        numberOfRooms++;
    }
    //printf("room number finded and it equal to %d!\n", numberOfRooms);
    currRoom = numberOfRoom;
    pthread_mutex_lock(&writeMutex);
    for (int i = 0; i < MAX_NUMBER_OF_CLIENTS; i++)
    {
        if (clients[i].sock != -1 && i != numberOfSender)
        {
            int currSock = clients[i].sock;
            send(currSock, &lengthOfMessage, sizeof(int), 0);
            send(currSock, &lengthOfName, sizeof(int), 0);
            send(currSock, &numberOfRooms, sizeof(int), 0);
            for (int j = 0; j < numberOfRooms; j++)
            {
                send(currSock, &currRoom, sizeof(int), 0);
                if (rooms[currRoom] != -1)
                    currRoom = rooms[currRoom];
            }
            int alreadySend = 0, nextPart;
            while (lengthOfName > alreadySend)
            {
                nextPart = send(currSock, nameOfSender + alreadySend, lengthOfName - alreadySend, 0);
                alreadySend += nextPart;
            }
            alreadySend = 0;
            while (lengthOfMessage > alreadySend)
            {
                nextPart = send(currSock, message + alreadySend, lengthOfMessage - alreadySend, 0);
                alreadySend += nextPart;
            }
        }
    }
    pthread_mutex_unlock(&writeMutex);
}

void sendToAll(int numberOfSender, int numberOfRoom, char * message)
{
    arg.clientNumber = numberOfSender;
    arg.room = numberOfRoom;
    arg.message = message;
    pthread_t sender;
    pthread_create(&sender, NULL, sendMessage, (void *)&arg);
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
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[2]));
    addr.sin_addr.s_addr = inet_addr(argv[1]);
    for(int i = 0; i < MAX_NUMBER_OF_CLIENTS; i++)
    {
        clients[i].sock = -1;
    }
    for (int i = 0; i < MAX_NUMBER_OF_ROOMS; i++)
    {
        rooms[i] = -1;
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
                    return 0;
                }
                for(int j = 0; j < MAX_NUMBER_OF_CLIENTS; j++)
                {
                	if(clients[j].sock == -1)
                	{
                        clients[j].name = (readMessageFrom(sock))->message;
                        fprintf(stderr, "%s connected\n", clients[j].name);
                	    clients[j].sock = sock;
                		activeUsers++;
                		break;
                	}
                }
                ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
                ev.data.fd = sock;
                if (epoll_ctl(epollFD, EPOLL_CTL_ADD, sock, &ev) == -1)
                {
                    perror("epoll_ctl: conn_sock");
                    return 0;
                }
            }
            else
            {
               if (events[i].events & EPOLLRDHUP)
               {
                    for(int j = 0; j < MAX_NUMBER_OF_CLIENTS; j++)
                    {
                		if(clients[j].sock == sock)
                		{
                			clients[j].sock = -1;
                			activeUsers--;
                			fprintf(stderr, "%s disconnected\n", clients[j].name);
                			free(clients[j].name);
                			break;
                		}
                	}
                    continue;
                }
                int numberOfSender = 0;
                for(int j = 0; j < MAX_NUMBER_OF_CLIENTS; j++)
                {
                    if(clients[j].sock == sock)
                    {
                        numberOfSender = j;
                        break;
                    }
                }
        		struct mess * readMessage = readMessageFrom(sock);
        		if (readMessage == NULL)
                    continue;
        		int currRoom = readMessage->room;
                int lengthOfMessage = strlen(readMessage->message);
                char *message = readMessage->message;
        		free(readMessage);
        		printf("%s\n", message);
        		sendToAll(numberOfSender, currRoom, message);
            }
        }
    }
    return 0;
}
