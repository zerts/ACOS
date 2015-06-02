#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>

const int NUMBER_OF_NUMBERS = 1000, NUMBER_OF_PARTS = 5, NUMBER_OF_PRINTED = 10, NUMBER_OF_NUMBER_IN_FILE = 1000000;
ssize_t size;
int currsize = 0, error;
void * addr;

void detectError(const char * err)
{
    perror(err);
    errno = 0;
}

int charIsDigit(char curr)
{
    if (curr <= '9' && curr >= '0')
        return 1;
    return 0;
}

struct pair
{
    int first;
    int second;
};

int comparePairs(void * a, void * b)
{
    int first, second;
    first = ((struct pair *)a)->first;
    second = ((struct pair *)b)->first;
    if (first > second)
        return -1;
    if (first == second)
        return 0;
    return 1;
}

void * func(void * arg)
{
	int part = *(int*)arg, tab = part * (size / NUMBER_OF_PARTS), currBegin = tab, oneMoreNumber = 0;
    char * currNumber = malloc(5);
    memset(currNumber, ' ', 4);
    currNumber[4] = '\0';
    if (currNumber == NULL)
    {
        detectError("malloc failed with string");
        return 0;
    }
    int currNumberInt = -1, lengthOfCurrNumber = 0;
    int * currResult = (int *)malloc(NUMBER_OF_NUMBERS * sizeof(int));
    if (currResult == NULL)
    {
        detectError("malloc failed with array of int");
        exit(0);
    }
    for (int i = 0; i < NUMBER_OF_NUMBERS; i++)
        currResult[i] = 0;

    if (part > 0 && charIsDigit(*(char*)(addr + tab)))
    {
        while (charIsDigit(*(char*)(addr + tab)))
            tab++;
        tab++;
    }
    int sizeOfCurrPart = size / NUMBER_OF_PARTS;
    if (part == NUMBER_OF_PARTS - 1)
        sizeOfCurrPart = size - (size / NUMBER_OF_PARTS) * (NUMBER_OF_PARTS - 1);
    while (tab - currBegin < sizeOfCurrPart || ((tab - currBegin >= size / NUMBER_OF_PARTS) && (charIsDigit(*(char*)(addr + tab)))))
    {
        if (tab - currBegin >= size / NUMBER_OF_PARTS)
            oneMoreNumber = 1;
        if (*(char*)(addr + tab) == ' ')
        {
            currNumberInt = atoi(currNumber);
            lengthOfCurrNumber = 0;
            //free(currNumber);
            //currNumber = malloc(5);
            memset(currNumber, ' ', 4);
            currNumber[4] = '\0';
            currResult[currNumberInt]++;
        }
        else
        {
            currNumber[lengthOfCurrNumber] = *(char*)(addr + tab);
            lengthOfCurrNumber++;
        }
        if (lengthOfCurrNumber < 0 || lengthOfCurrNumber > 4)
        {
            detectError("strange number");
            exit(0);
        }
        tab++;
    }
    if (oneMoreNumber)
    {
        currNumberInt = atoi(currNumber);
        lengthOfCurrNumber = 0;
        //free(currNumber);
        //currNumber = malloc(5);
        currResult[currNumberInt]++;
        //free(currNumber);
    }
    free(currNumber);
    return (void *)currResult;
}

int main()
{
	FILE * f = fopen("file", "w");
	if (f == NULL)
	{
        detectError("fopen failed");
        return 0;
	}
    for (int i = 0; i < NUMBER_OF_NUMBER_IN_FILE; i++)
    {
        fprintf(f, "%d", rand() % NUMBER_OF_NUMBERS);
        if (i != NUMBER_OF_NUMBER_IN_FILE - 1)
            fprintf(f, " ");
    }
	fprintf(f, "\n");
	fclose(f);

	int fd = open("file", O_CREAT | O_RDWR, 0777);
	if (fd == -1)
	{
        detectError("open failed");
        return 0;
	}

	struct stat sizeOfFile;
	stat("file", &sizeOfFile);
	size = (ssize_t)sizeOfFile.st_size;

	if (size <= 0)
	{
        detectError("bad size of file");
        return 0;
	}

    int result[NUMBER_OF_NUMBERS];
	for (int i = 0; i < NUMBER_OF_NUMBERS; i++)
		result[i] = 0;

	addr = mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
	if (addr == (void *)(-1))
	{
        detectError("mmap failed");
        return 0;
	}

	int currsize = 0;
	pthread_t t[NUMBER_OF_PARTS];


    int * arg = malloc(NUMBER_OF_PARTS * sizeof(int));
    for (int i = 0; i < NUMBER_OF_PARTS; i++)
        arg[i] = i;

	for (int i = 0;  i < NUMBER_OF_PARTS; i++)
	{
		if (pthread_create(&t[i], NULL, (void *)&func, (void *)&arg[i]))
		{
            detectError("pthread_creat failed");
            return 0;
		}
    }

    for (int i = 0; i < NUMBER_OF_PARTS; i++)
    {
        int * currResult;
        if ( (pthread_join(t[i], (void *)&currResult)) )
        {
            detectError("pthread_join failed");
            return 0;
        }
        for (int j = 0; j < NUMBER_OF_NUMBERS; j++)
            result[j] += currResult[j];
        free(currResult);
    }

    struct pair * resultPairs = malloc(NUMBER_OF_NUMBERS * sizeof(struct pair));
    if (resultPairs == NULL)
    {
        detectError("creating pair array failed");
        return 0;
    }
    int total = 0;
    for (int i = 0; i < NUMBER_OF_NUMBERS; i++)
    {
        total += result[i];
        resultPairs[i].first = result[i];
        resultPairs[i].second = i;
    }
    printf("total = %d\n", total);
    qsort(resultPairs, NUMBER_OF_NUMBERS, sizeof(struct pair), comparePairs);

    for (int i = 0; i < NUMBER_OF_PRINTED; i++)
    {
        printf("№%d - %d \n", i, resultPairs[i].second);
    }

    free(arg);
    free(resultPairs);
    munmap(addr,  size);
	close(fd);
    return 0;
}
