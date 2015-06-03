#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

struct arg
{
    char name[MAX_NAME_SIZE];
    int size;
    void * memory;
};

struct arg_vector
{
    arg * args;
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
            memcpy((args->args[i]).memory, currArg->memory, currArg->size);
            return;
        }
    }
    args = realloc(args->size * 2 * sizeof(struct arg));
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
    for (int i = 0; i < agrs->size; i++)
    {
        if (strcmp(name, (args->args[i]).name) == 0)
        {
            return &((agrs->args)[i]);
        }
    }
    return NULL;
}

int auth(struct arg_vector *args)
{
    srand(time(0));
    int theNumber = rand();
    struct arg currArg;
    struct arg * goodArg;
    if ((goodArg = get_arg(args, "rememberTest")) == NULL)
    {
    	memcpy(currArg.name, "rememberTest", strlen("rememberTest"));
    	currArg.size = sizeof(int);
    	memcpy(currArg.memory, (void *)&theNumber, sizeof(int));
    	put_arg(agrs, &currArg);
    }
	else
	{
		goodArg->size = sizeof(int);
		memcpy(goodArg->memory, (void *)&theNumber, sizeof(int));
	}
    printf("please, remember this!: %d\n", theNumber);
    return 1;
}
