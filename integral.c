#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <errno.h>
#include <sys/types.h>

extern int errno;

void detectError(const char * err)
{
    perror(err);
    errno = 0;
}

float integral(double left, double right, char * fileName, char * functionName)
{
    void * lib = dlopen(fileName, RTLD_LAZY);
    if (lib == NULL)
    {
        detectError("No such library");
        exit(0);
    }
    double (*f)(double) = dlsym(lib, functionName);
    if (f == NULL)
    {
        detectError("Bad function");
        exit(0);
    }
    double result = 0., _left, _right;
    for (int i = 0; i < 1000; i++)
    {
        _left = left + ((right - left) / 1000.) * (double)i;
        _right = _left + ((right - left) / 1000.);
        result += (_right - _left) * f((_left + _right) / 2.)
    }
    if(dlclose(lib) != 0)
    {
        detectError("dlclose failed");
        exit(0);
    }
    return result;
}

int main(int argc, char **argv)
{
    int numberOfParts = 4;
    if (argc != 5)
    {
        detectError("Not all arguments");
        return 0;
    }
	int intleft = atoi(argv[1]), intright = atoi(argv[2]);
	char * fileWithFunctions = argv[3], * functionName = argv[4];
	int pipefd[5][2];
	double left = (double)intleft, right = (double)intright;

    for (int i = 0; i < numberOfParts; i++)
    {
        if (pipe(pipefd[i]) == -1)
        {
            detectError("pipe faild");
            return 0;
        }
    }

	for (int part = 0; part < numberOfParts; part++)
	{
        double currLeft = left + (((right - left) / numberOfParts) * part), currRight = left + (((right - left) / numberOfParts) * (part + 1));
        pid_t t = fork();
        if (t == -1)
        {
            detectError("fork failed");
            return 0;
        }
        if (t == 0)
        {
            double result = integral(currLeft, currRight, fileWithFunctions, functionName);
            int doubleSize = sizeof(double);
            if (write(pipefd[part][1], &result, doubleSize) == -1)
            {
                detectError("write failed");
                return 0;
            }
            if (close(pipefd[part][1]) == -1)
            {
                detectError("close of oioe failed");
                return 0;
            }
            return 0;
        }
	}
	double sum = 0;
	for (int i = 0; i < numberOfParts; i++)
	{
        if (close(pipefd[i][1]) == -1)
            {
                detectError("close of oioe failed");
                return 0;
            }
        double resultOfCurrPart;
        int doubleSize = sizeof(double);
        if (read(pipefd[i][0], &resultOfCurrPart, doubleSize) == -1)
        {
            detectError("read failed");
            return 0;
        }
        sum += resultOfCurrPart;
	}
	if (intleft > intright)
        sum *= -1;
	printf("%f\n", sum);
    return 0;
}

//compile: gcc integral.h -std=gnu99 -ldl
//run: ./a.out 0 1 /lib/x86_64-linux-gnu/libm.so.6 cos

