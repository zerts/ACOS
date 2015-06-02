#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

extern int errno;
const int BUF_SIZE = 256;
char * buf, * currString;
int sizeOfTemp, currStringLength, currStringSize;

void detectError(const char *err)
{
    perror(err);
    errno = 0;
}

int currentStringExists()
{
    free(currString);
    currString = malloc(21 * sizeof(char));
    if (currString == NULL)
    {
        detectError("malloc failed in creation currString");
        return 0;
    }
    currString[0] = '\0';
    currStringLength = 0;
    currStringSize = 20;
    free(buf);
    buf = malloc(BUF_SIZE);
    if (buf == NULL)
    {
        detectError("malloc failed in creating buffer");
        return 0;
    }
    return 1;
}

int has(char* temp)
{
    for (int j = 0; j < currStringLength; j++)
    {
        for (int i = 0; i < sizeOfTemp; i++)
        {
            if (i + j >= currStringLength)
                break;
            if (i + j >= strlen(currString))
            {
                printf("bad in has! %d, %d\n", i + j, (int)strlen(currString));
            }
            if (currString[i + j] != temp[i])
            {
                break;
            }
            if (i == sizeOfTemp - 1)
            {
                return 1;
            }
        }
    }
    return 0;
}

int pushCharToString(char currChar)
{
    if (currStringLength + 1 == currStringSize)
    {
    printf("new size = %d\n", (2 * currStringSize + 1));
    printf("strlen of currString before realloc = %d\n", (int)strlen(currString));
    printf("new currStringSize is = %d\n", 2 * currStringSize);
        currString = (char *)realloc(currString, (2 * currStringSize + 1) * sizeof(char));
    printf("strlen of currString after realloc = %d\n", (int)strlen(currString));
        currStringSize *= 2;
        if (currString == NULL)
        {
            detectError("realloc failed in changing currString's size");
            return 0;
        }
    }
    //printf("strlen of currString = %d\n", strlen(currString));
    //printf("currStringLength is = %d\n", currStringLength);
    printf("strlen of currString = %d\n", (int)strlen(currString));
    currString[currStringLength] = currChar;
    currStringLength++;
    currString[currStringLength] = '\0';
}

int writeBuf(int fd, const void *buffer, size_t numberOfChars)
{
    int numberOfWriteBytes;
    int total = 0;
    while (total < numberOfChars)
    {
        numberOfWriteBytes = write(fd, buffer + total, numberOfChars - total);
        if (numberOfWriteBytes == -1)
        {
            detectError("write failed in writing buffer");
            return 0;
        }
        total += numberOfWriteBytes;
    }
    return 1;
}

int main(int argc, char** argv)
{
    char currChar, * temp = argv[1];
    if (temp == NULL)
    {
        detectError("No template");
        return 0;
    }
    if (!currentStringExists())
        return 0;
    sizeOfTemp = strlen(temp);
    int resultOfRead;
    while ((resultOfRead = read(STDIN_FILENO, buf, BUF_SIZE * sizeof(char))))
    {
        if (resultOfRead == -1)
        {
            detectError("read failed");
            free(temp);
            free(buf);
            free(currString);
            return 0;
        }
        printf("resultOfRead = %d\n", resultOfRead);
        for (int i = 0; i < resultOfRead; i++)
        {
            char currChar = buf[i];
            if (!pushCharToString(currChar))
            {
                free(temp);
                free(buf);
                free(currString);
                return 0;
            }
            if (currChar == '\n' && currStringSize >= strlen(temp))
            {
                if (has(temp))
                {
                    if (!writeBuf(STDOUT_FILENO, currString, currStringLength * sizeof(char)))
                    {
                        free(temp);
                        free(buf);
                        free(currString);
                        return 0;
                    }
                }
                if (!currentStringExists())
                {
                    free(temp);
                    return 0;
                }
            }
        }
    }
    free(temp);
    free(buf);
    return 0;
}
