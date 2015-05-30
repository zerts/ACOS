#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <sys/epoll.h>

const int MAX_SIZE = 2048;
char * result;

extern int errno;

void detectError(char * err)
{
    perror(err);
    errno = 0;
}

char * readError(int fd)
{
    result[0] = '\0';
    int tab = 0, curr = 0;
    while ((curr = read(fd, result + tab, MAX_SIZE - tab - 1)) != 0)
    {
        if (curr == -1)
        {
            detectError("read failed");
            exit(0);
        }
        if (result[tab + curr - 1] == '\n')
        {
            result[tab + curr - 1] = '\0';
            return result;
        }
        tab += curr;
    }
    return result;
}

int main(int argc, char ** argv)
{
    result = malloc(MAX_SIZE * sizeof(char));
    int pipes[argc + 1][2], errors[argc + 1][2];
    int numberOfProgs = argc - 1;
    struct epoll_event currError;
    struct epoll_event *currErrors = (struct epoll_event *)malloc(argc * sizeof(struct epoll_event));
    int epolfd = epoll_create(argc - 1);
    int activeChildren = argc - 1;
    if (currErrors == NULL)
    {
        detectError("malloc failed");
        return 0;
    }
    for (int i = 1; i < argc; i++)
    {
        if (pipe(pipes[i]) == -1 || pipe(errors[i]) == -1)
        {
            detectError("pipe failed");
            return 0;
        }
    }
    pid_t children[argc + 1];
    for (int i = 1; i < argc; i++)
    {
        children[i] = fork();
        if (children[i] == 0)
        {
            if (close(pipes[i][0]) == -1 || close(errors[i][0]) == -1)
            {
                detectError("close failed");
                return 0;
            }
            if (i > 1)
            {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1)
                {
                    detectError("dub2 failed");
                    return 0;
                }
            }
            if (i + 1 != argc)
            {
                if (dup2(pipes[i][1], STDOUT_FILENO) == -1)
                {
                    detectError("dub2 failed");
                    return 0;
                }
            }
            if (dup2(errors[i][1], STDERR_FILENO) == -1)
            {
                detectError("dub2 failed");
                return 0;
            }
            if (execl("/bin/bash", "/bin/bash", "-c", argv[i], NULL) == -1)
            {
                detectError("execl failed");
                return 0;
            }
            return 0;
        }
        else if (children[i] == -1)
        {
            detectError("fork failed");
            exit(0);
        }
        if (close(pipes[i][1]) == -1 || close(errors[i][1]) == -1)
        {
            detectError("close failed");
            return 0;
        }
        currError.data.fd = errors[i][0];
        if (epoll_ctl(epolfd, EPOLL_CTL_ADD, errors[i][0], &currError) == -1)
        {
            detectError("epoll_ctl failed");
            return 0;
        }
    }
    while (activeChildren > 0)
    {
        int returnValue = 0;
        int resultOfPid = waitpid(-1, &returnValue, WNOHANG);
        if (resultOfPid == -1)
        {
            detectError("waitpid failed");
            return 0;
        }
        if (resultOfPid != 0)
        {
            activeChildren--;
            if (returnValue != 0)
            {
                fprintf(stderr, "Child #%d had finished and return %d status\n", resultOfPid, returnValue);
                return 0;
            }
        }
        int numberOfEpolFD = epoll_wait(epolfd, currErrors, argc - 1, -1);
        if (numberOfEpolFD == -1)
        {
            detectError("epoll_wait failed");
            return 0;
        }
        for (int i = 0; i < numberOfEpolFD; i++)
        {
            for (int j = 1; j < argc; j++)
            {
                if (currErrors[i].data.fd == errors[j][0])
                {
                    char *outstr = readError(currErrors[i].data.fd);
                    if (strlen(outstr) != 0)
                    {
                        fprintf(stderr, "%s: %s\n", argv[j], outstr);
                        fflush(stderr);
                    }
                    break;
                }
            }
        }
    }
    free (currErrors);
    return 0;
}
