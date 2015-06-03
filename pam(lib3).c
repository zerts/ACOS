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
            memcpy((args->args[i]).memory, currArg->memory, currArg->size * sizeof(char));
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
    int baseNumber, userNumber;
    struct arg * currArg;
    if ((currArg = get_arg(args, "rememberTest")) == NULL)
    {
    	return 1;
    }
	else
	{
		baseNumber = *(int*)(currArg->memory);
	}
	scanf("%d", &userNumber);
    return (baseNumber == userNumber);
}
