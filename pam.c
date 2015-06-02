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
#include <dlfcn.h>

#define MAX_NAME_SIZE 30

extern int errno;

void detectError(char * err)
{
    perror(err);
    errno = 0;
}

struct arg
{
    char name[MAX_NAME_SIZE];
    int size;
    void * memory;
};

struct arg_vector
{
    struct arg * args;
    int size;
};

void put_arg(struct arg_vector * args, struct arg * currArg)
{
    for (int i = 0; i < args->size; i++)
    {
        if (((args->args[i]).name)[0] == '\0')
        {
            memcpy((args->args[i]).name, currArg->name, MAX_NAME_SIZE);
            (args->args)[i].size = currArg->size;
            memcpy((args->args[i]).memory, currArg->memory, currArg->size * sizeof(char));
            return;
        }
    }
    args = realloc(args, args->size * 2 * sizeof(struct arg));
    for (int j = 0; j < MAX_NAME_SIZE; j++)
        ((args->args[args->size]).name)[j] = (currArg->name)[j];
    (args->args)[args->size].size = currArg->size;
    (args->args[args->size]).memory = currArg->memory;
    args->size *= 2;
    return;
}

struct arg * get_arg(struct arg_vector * args, char * name)
{
    struct arg * currArg;
    for (int i = 0; i < args->size; i++)
    {
        if (strcmp(name, (args->args[i]).name) == 0)
        {
            return &((args->args)[i]);
        }
    }
    return NULL;
}

struct arg_vector args;

void init_args()
{
    free(args.args);
    args.args = malloc(10 * sizeof(struct arg));
    args.size = 10;
    for (int i = 0; i < 10; i++)
    {
        args.args[i].name[0] = '\0';
        args.args[i].size = 0;
        free(args.args[i].memory);
    }
}

int check(char *pathToScript)
{
    init_args();
    char libName[2048];
//     printf("HI\n");
    FILE *fileWithCheck = fopen(pathToScript, "r");
    int lenOfFirstWord;

    while (fgets(libName, 2047, fileWithCheck))
    { 
        char path[2048], theFirstWord[2048];
        sprintf(theFirstWord, "%s", libName);
        printf("the first word = %s\n", theFirstWord);
        if (strstr(theFirstWord, "lib") == NULL)
            continue;
        char currChar;
        lenOfFirstWord = strlen("lib");
        do
        {
            sscanf (libName + strlen(theFirstWord), "%c", currChar);
            lenOfFirstWord++;
        }
        while (currChar == ' ');
        memcpy(path, "libs/", strlen("libs/"));
        sprintf(path + strlen("libs/"), "%s", libName + lenOfFirstWord);
        path[strlen(path) - 1] = '\0';
        void *fileWithFunc = dlopen(path, RTLD_LAZY);
        if (fileWithFunc == 0)
        {
            detectError("dlopen failed");
            exit(0);
        }
        int (*f)(struct arg *) = dlsym(fileWithFunc, "auth");
        if (f == NULL)
        {
            detectError("dlsym failed");
            exit(0);
        }
        if (f(&args) == 0)
        {
            fclose(fileWithCheck);
            return 0;
        }
        dlclose(fileWithFunc);
    }
    fclose(fileWithCheck);
    return 1;
}

int main()
{
    if (check("file") == 1)
        printf("Ok!\n");
    else
        printf("You are not allowed!\n");
    return 0;
}
