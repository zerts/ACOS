#include <stdio.h>
#include <unistd.h>

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

int auth(struct arg_vector *args)
{
    return (getgid() == 1000);
}
