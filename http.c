#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

#define PROCESSORS_NUMBER 4
#define QUEUE_SIZE 100

extern int errno;

const int MAX_SIZE_OF_BUFFER = 8192;
const int NUMBER_OF_LISTENERS = 4;
int queue[QUEUE_SIZE];
int beginOfQue = 0;
int endOfQue = 0;
char* statuses[600];
pthread_mutex_t mutex, totalMutex;

void detectError(char * err)
{
    perror(err);
    errno = 0;
}

void initStatuses()
{
    statuses[200] = (char *)(malloc(strlen("OK")));
    statuses[404] = (char *)(malloc(strlen("Not found")));
    statuses[400] = (char *)(malloc(strlen("Bad request")));
    statuses[403] = (char *)(malloc(strlen("Forbidden")));
    if (statuses[200] == NULL)
    {
        detectError("1malloc failed");
        exit(0);
    }
    if (statuses[404] == NULL)
    {
        detectError("2malloc failed");
        exit(0);
    }
    if (statuses[400] == NULL)
    {
        detectError("3malloc failed");
        exit(0);
    }
    if (statuses[403] == NULL)
    {
        detectError("3malloc failed");
        exit(0);
    }
    statuses[200] = "OK";
    statuses[404] = "Not found";
    statuses[400] = "Bad request";
    statuses[403] = "Forbidden";
}

int sizeOfQueue()
{
    return (endOfQue - beginOfQue + QUEUE_SIZE) % QUEUE_SIZE;
}

char BAD_REQUEST[] = "<html><h1>400 BAD REQUEST</h1></html>\r\n";
char INTERNAL_SERVER_ERROR[] = "<html><h1>500 INTERNAL SERVER ERROR</h1></html>\r\n";
char NOT_FOUND[] = "<html><h1>404 NOT FOUND</h1></html>\r\n";
char FORBIDDEN[] = "<html><h1>403 FORBIDDEN</h1></html>\r\n";

void sendAll(int sd, char *message)
{
    int messageLength = strlen(message);
    int alreadySend = 0, nextPart;
    while (alreadySend < messageLength)
    {
        nextPart = send(sd, message + alreadySend, messageLength - alreadySend, 0);
        if (nextPart == -1)
        {
            detectError("send failed");
            exit(0);
        }
        alreadySend += nextPart;
    }
}

void writeError(FILE *sd, int status, char *typeOfFile, char *message)
{
    int printed = fprintf(sd, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", status, statuses[status], typeOfFile, strlen(message));
    printed = fprintf(sd, "%s\r\nConnection: Close\r\n", message);
    if (fflush(sd) == EOF)
    {
        detectError("fflush failed");
        exit(0);
    }
    fprintf(stderr, "ready\n");
}

void sendFile(FILE *sd, char *path)
{
    int curr = 0;
    char buf[MAX_SIZE_OF_BUFFER];
    int fd = open(path, O_RDONLY);
    while (curr = read(fd, buf, MAX_SIZE_OF_BUFFER))
    {
        if (curr == -1)
        {
            //writeError(sd, 403, "text/html", FORBIDDEN);
            errno = 0;
            return;
        }
        if (curr * sizeof(char) != fwrite(buf, sizeof(char), curr, sd))
        {
            detectError("fwrite failed");
            exit(1);
        }
    }
    close(fd);
    if (fflush(sd) == EOF)
    {
        detectError("fflush failed");
        exit(0);
    }
    fprintf(stderr, "SENDED\n");

}

void *worker (void *args)
{
    while(1)
    {
        int currElem = -1;
        if (pthread_mutex_lock(&mutex) == -1)
        {
            detectError("pthread_mutex_lock failed");
            exit(0);
        }
        if (sizeOfQueue())
        {
            currElem = queue[beginOfQue++];
            if (beginOfQue == QUEUE_SIZE)
                beginOfQue = 0;
        }
        if (pthread_mutex_unlock(&mutex) == -1)
        {
            detectError("pthread_mutex_unlock failed");
            exit(0);
        }

        if (currElem == -1)
        {
            if (pthread_yield() == -1)
            {
                detectError("pthread_yield failed");
                exit(0);
            }
        }
        else
        {
            fprintf(stderr, "Taken %d\n", currElem);
            int total = 0;
            int current = 0;
            int checked = 0;

            FILE *buff = fdopen(currElem, "r");
            if (buff == NULL)
            {
                detectError("fdopen failed");
                exit(0);
            }
            FILE *outFile = fdopen(currElem, "w");

            char method[MAX_SIZE_OF_BUFFER];
            char filePath[MAX_SIZE_OF_BUFFER];
            char protocol[MAX_SIZE_OF_BUFFER];
            int value = fscanf(buff, "%s %s %s", method, filePath, protocol);
            if (value != 3 || strcmp(protocol, "HTTP/1.1") || (strcmp(method, "POST") && strcmp(method, "GET")))
            {
                writeError(outFile, 400, "text/html", BAD_REQUEST);
            }
            else
            {
                fprintf(stderr, "%s %s %s\n", method, filePath, protocol);
            }
            int curr;
            for (int i = 0; i < MAX_SIZE_OF_BUFFER; i++)
            {
                if (!(filePath[i] && filePath[i] != '?'))
                {
                    curr = i;
                    break;
                }
            }
            filePath[curr] = '\0';
            for (int i = curr; i >= 0; i--)
                filePath[i + 1] = filePath[i];
            filePath[0] = '.';
            filePath[1] = '/';
            printf("%s\n", filePath);
            struct stat fileInfo;
            printf("%d\n", lstat(filePath, &fileInfo));
            if (lstat(filePath, &fileInfo) == -1)
            {
                printf("bad file errno = %d!\n", errno);
                if (errno == EACCES)
                {
                    writeError(outFile, 403, "text/html", FORBIDDEN);
                    errno = 0;
                }
                else if (errno == ENOENT)
                {
                    writeError(outFile, 404, "text/html", NOT_FOUND);
                    errno = 0;
                }
                else
                {
                    detectError("lstat failed");
                    exit(0);
                }
            }
            else if (access(filePath, R_OK) == -1)
            {
                    writeError(outFile, 403, "text/html", FORBIDDEN);
                    errno = 0;

            }
            else if (S_ISDIR(fileInfo.st_mode & S_IFMT) == 0)
                    sendFile(outFile, filePath);

            fclose(buff);
            fclose(outFile);
        }
    }
    return NULL;
}


int main(int argc, char ** argv)
{
    initStatuses();
    if (pthread_mutex_init(&mutex, NULL) == -1)
    {
        detectError("pthread_mutex_init  failed");
        return 0;
    }
    if (pthread_mutex_init(&totalMutex, NULL) == -1)
    {
        detectError("pthread_mutex_init  failed");
        return 0;
    }
    if (argc <= 2)
    {
        fprintf(stderr, "Usage host port\n");
        return 0;
    }
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0)
    {
        detectError("socket failed");
        return 0;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[2]));
    addr.sin_addr.s_addr = inet_addr(argv[1]);
    int optVal = 1;
    socklen_t optLen = sizeof(optVal);
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (void*) &optVal, optLen) == -1)
    {
        detectError("setsockopt failed");
        return 0;
    }
    if(bind(sd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        detectError("bind failed failed");
        return 0;
    }

    listen(sd, QUEUE_SIZE);

    struct epoll_event epollEvent, events[QUEUE_SIZE];
    int epfd = epoll_create(QUEUE_SIZE);

    if (epfd == -1)
    {
        detectError("epoll_create failed");
        return 0;
    }
    epollEvent.events = EPOLLIN;
    epollEvent.data.fd = sd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sd, &epollEvent) == -1)
    {
        detectError("epoll_ctl failed");
        return 0;
    }
    pthread_t workers[NUMBER_OF_LISTENERS];
    fprintf(stderr, "Starting creating workers\n");
    for (int i = 0; i < NUMBER_OF_LISTENERS; ++i)
    {
        int err= pthread_create(&workers[i], NULL, worker, NULL);
        if (err != 0 )
        {
            fprintf(stderr, "pthread_create: (%d)%s\n", err, strerror(err));
            return 0;
        }
    }
    fprintf(stderr, "Worker created!\n");

    while (1)
    {
        int ndfs = epoll_wait(epfd, events, QUEUE_SIZE, -1);
        if (ndfs == -1)
        {
            detectError("epoll_wait failed");
            return 1;
        }
        fprintf(stderr, "new event!\n");
        if (ndfs == -1)
        {
            detectError("epoll_wait failed");
            return 0;
        }

        for (int i = 0; i < ndfs; ++i)
        {
            if (events[i].data.fd == sd)
            {
                int sock = accept(sd, NULL, NULL);
                fprintf(stderr, "connect: %d\n", sock);
                if(sock < 0)
                {
                    detectError("accept failed");
                    return 0;
                }
                if (pthread_mutex_unlock(&totalMutex) == -1)
                {
                    detectError("pthread_mutex_unlock");
                    return 0;
                }
                epollEvent.events = EPOLLIN | EPOLLET;
                epollEvent.data.fd = sock;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &epollEvent) == -1)
                {
                    detectError("epoll_ctl: conn_sock");
                    return 0;
                }
            }
            else
            {
                fprintf(stderr, "put to query: %d\n", events[i].data.fd);
                if (pthread_mutex_lock(&mutex) == -1)
                {
                    detectError("pthread_mutex_lock");
                    return 0;
                }
                if (endOfQue == QUEUE_SIZE)
                    endOfQue = 0;
                queue[endOfQue] = events[i].data.fd;
                ++endOfQue;
                if (endOfQue == beginOfQue)
                {
                    if (pthread_mutex_lock(&totalMutex) == -1)
                    {
                        detectError("pthread_mutex_unlock");
                        return 0;
                    }
                }
                if (pthread_mutex_unlock(&mutex) == -1)
                {
                    detectError("pthread_mutex_unlock");
                    return 0;
                }
            }
        }
    }
}
