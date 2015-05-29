#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

extern int errno;
const int BUF_SIZE = 100;
char * buf, * currString;
int sizeOfTemp, currStringLength, currStringSize;

void detectError(const char *err)
{
    perror(err);
    errno = 0;
}

int currentStringExists()
{
    currString = (char *)malloc(101 * sizeof(char));
    if (currString == NULL)
    {
        detectError("malloc failed in creation currString");
        return 0;
    }
    currString[0] = '\0';
    currStringLength = 0;
    currStringSize = 100;
    return 1;
}

void clearCurrString()
{
    currStringLength = 0;
    currString[0] = '\0';
}

int has(char* temp)
{
    for (int j = 0; j < currStringLength; j++)
    {
        for (int i = 0; i < sizeOfTemp; i++)
        {
            if (j + i >= currStringLength)
            {
                break;
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
    if (currStringLength == currStringSize)
    {
        currString = (char *)realloc(currString, (2 * currStringSize + 1) * sizeof(char));
        currStringSize *= 2;
        if (currString == NULL)
        {
            detectError("realloc failed in changing currString's size");
            return 0;
        }
    }
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
    buf = malloc(BUF_SIZE);
    if (buf == NULL)
    {
        detectError("malloc failed in creating buffer");
        return 0;
    }
    char currChar, * temp = argv[1];
    if (temp == NULL)
    {
        detectError("No template");
        return 0;
    }
    if (!currentStringExists())
        return 0;
    sizeOfTemp = strlen(temp);
    currString = malloc(sizeOfTemp + 1);
    int resultOfRead;
    while ((resultOfRead = read(STDIN_FILENO, buf, BUF_SIZE * sizeof(char))))
    {
        if (resultOfRead == -1)
        {
            detectError("read failed");
            return 0;
        }
        for (int i = 0; i < resultOfRead; i++)
        {
            char currChar = buf[i];
            if(!pushCharToString(currChar))
            {
                return 0;
            }
            if (currChar == '\n')
            {
                if (has(temp))
                {
                    if (!writeBuf(STDOUT_FILENO, currString, currStringLength * sizeof(char)))
                    {
                        return 0;
                    }
                }
                clearCurrString();
            }
            else
            {

            }
        }
    }
    free(temp);
    free(buf);
    free(currString);
    return 0;
}
