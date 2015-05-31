#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>

#define MAX_NUMBER_OF_ROOMS 1024
#define MAX_NAME_SIZE 30

#define BUF_SIZE 1024

void detectError(char * err)
{
    perror(err);
}

pthread_mutex_t forRead, forWrite;
char * name;
int rooms[MAX_NUMBER_OF_ROOMS];
int numberOfRooms, currRoom;
int sock;

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
    while (sizeof(int) > alreadySend)
    {
        nextPart = send(sock, &currRoom + alreadySend, sizeof(int) - alreadySend, flag);
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

void parse_command(char * input)
{
    if (strstr(input, "-get_room") != NULL)
    {
        char * numberStr = input + 9;
        int numberOfRoom = atoi(numberStr);
        for (int i = 0; i < numberOfRooms; i++)
        {
            if (rooms[i] == numberOfRoom)
            {
                printf("already in this room\n");
                return;
            }
        }
        if (numberOfRoom < 0)
        {
            printf("bad room number\n");
            return;
        }
        rooms[numberOfRooms] = numberOfRoom;
        numberOfRooms++;
    }
    else if (strstr(input, "-change_room") != NULL)
    {
        char * numberStr = input + 12;
        int numberOfRoom = atoi(numberStr);
        int inRoom = 0;
        for (int i = 0; i < numberOfRooms; i++)
            if (rooms[i] == numberOfRoom)
                inRoom = 1;
        if (inRoom == 0)
        {
            printf("you aren't in this room now\n");
            return;
        }
        currRoom = numberOfRoom;
    }
    else if (strstr(input, "-put_room") != NULL)
    {
        char * numberStr = input + 12;
        int numberOfRoom = atoi(numberStr);
        printf("And in which room?\n");
        char biggerRoomStr[3];
        int biggerRoom = atoi(biggerRoomStr);
        pthread_mutex_lock(&forWrite);
        int alreadySend = 0, nextPart;
        while (sizeof(int) > alreadySend)
        {
            nextPart = send(sock, &numberOfRoom + alreadySend, sizeof(int) - alreadySend, 0);
            alreadySend += nextPart;
        }
        alreadySend = 0;
        while (sizeof(int) > alreadySend)
        {
            nextPart = send(sock, &biggerRoom + alreadySend, sizeof(int) - alreadySend, 0);
            alreadySend += nextPart;
        }
        alreadySend = 0;
        char * output = malloc((numberOfRoom + 1) * sizeof(char));
        for (int i = 0; i < numberOfRoom; i++)
            output[i] = '0';
        output[numberOfRoom] = '\0';
        while (numberOfRoom + 1 > alreadySend)
        {
            nextPart = send(sock, output + alreadySend, numberOfRoom - alreadySend + 1, 0);
            alreadySend += nextPart;
        }
        pthread_mutex_unlock(&forWrite);
    }
}

int inRoom(int room)
{
    for (int i = 0; i < numberOfRooms; i++)
    {
        if (room == rooms[i])
            return 1;
    }
    return 0;
}

int listenTheMessage(int currSock, char * senderName, char * buf, int flag)
{
    int messageLength = 0, nameLength = 0;
    int alreadyRead = 0;
    int curr = 0;
    int returnRoom = -1;
    pthread_mutex_lock(&forRead);
    while (curr < sizeof(int))
    {
        int nextPart = recv(currSock, (char*)(&messageLength) + curr, sizeof(int) - curr, flag);
        if (nextPart < 0)
        {
            detectError("recv failed");
            exit(0);
        }
        curr += nextPart;
    }
    curr = 0;
    while (curr < sizeof(int))
    {
        int nextPart = recv(currSock, (char*)(&nameLength) + curr, sizeof(int) - curr, flag);
        if (nextPart < 0)
        {
            detectError("recv failed");
            exit(0);
        }
        curr += nextPart;
    }
    curr = 0;
    int numberOfGoodRooms, room;
    while (curr < sizeof(int))
    {
        int nextPart = recv(currSock, (char*)(&numberOfGoodRooms) + curr, sizeof(int) - curr, flag);
        if (nextPart < 0)
        {
            detectError("recv failed");
            exit(0);
        }
        curr += nextPart;
    }
    curr = 0;
    for (int i = 0; i < numberOfGoodRooms; i++)
    {
        while (curr < sizeof(int))
        {
            int nextPart = recv(currSock, (char*)(&room) + curr, sizeof(int) - curr, flag);
            if (nextPart < 0)
            {
                detectError("recv failed");
                exit(0);
            }
            curr += nextPart;
        }
        if (inRoom(room))
            returnRoom = room;
        curr = 0;
    }
    while (curr < nameLength)
    {
        int nextPart = recv(currSock, senderName + curr, nameLength - curr, flag);
        if (nextPart < 0)
        {
            detectError("recv failed");
            exit(0);
        }
        curr += nextPart;
    }
    curr = 0;
    while (curr < messageLength)
    {
        int nextPart = recv(currSock, buf + curr, messageLength - curr, flag);
        if (nextPart < 0)
        {
            detectError("recv failed");
            exit(0);
        }
        curr += nextPart;
    }
    pthread_mutex_unlock(&forRead);
    buf[messageLength] = '\0';
    return returnRoom;
}

void * listenAll(void * args)
{
    int currSock = *(int *)(args);
    char buf[BUF_SIZE], senderName[MAX_NAME_SIZE];
    while (1)
    {
        int retVal = listenTheMessage(currSock, senderName, buf, 0);
        if(retVal != -1)
            printf("room #%d, user \"%s\": %s\n", retVal, senderName, buf);
    }
    return NULL;
}

int main(int argc, char ** argv)
{
    name = malloc(30 * sizeof(char));
    name = argv[1];
    struct sockaddr_in addr;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        detectError("socket failed");
        exit(1);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[3]));
    addr.sin_addr.s_addr = inet_addr(argv[2]);
    if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        detectError("connect failed");
        exit(2);
    }
    rooms[0] = -1;
    currRoom = -1;
    numberOfRooms = 0;
    sendMessage(sock, name, strlen(name), 0);
    printf("Hello!\nNow choose your room with \"-get_room (number)\"\n");
    char input[1024];
    while (rooms[0] == -1)
    {
        gets(input);
        if (input[0] != '-')
        {
            printf("bad command\n");
            continue;
        }
        parse_command(input);
    }
    printf("Now you can change active room with \"-change_room (number of room)\"\n");
    currRoom = rooms[0];

    pthread_t listener;
    if(pthread_create(&listener, NULL, listenAll, (void *)(&sock)))
    {
    	perror("pthread_create failed");
    	return 0;
    }

    while(1)
    {
        gets(input);
        if (strcmp(input, "-exit") == 0)
            break;
        if (input[0] == '-')
            parse_command(input);
        else
        {
            sendMessage(sock, input, strlen(input), 0);
        }
    }
    close(sock);
    return 0;
}
