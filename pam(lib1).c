#include <stdio.h>
#include <string.h>
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

int auth(struct arg_vector *args)
{
	char s[20];
    printf("Password:");
    scanf("%s", s);
    if (strcmp(s, "admin") == 0)
        return 1;
    return 0;
}
