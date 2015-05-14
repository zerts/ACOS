#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

extern int errno;

void detectError(const char err)
{
    perror(err);
    errno = 0;
}

int main()
{
	char outstring[10], hello[7];
	time_t t;
	struct tm *time_template;

	int descr[2];
	if (pipe(descr) == -1)
	{
        detectError("pipe failed");
        return 0;
	}
	pid_t psIndicator = fork();
	if (psIndicator == -1)
	{
        detectError("fork failed");
        return 0;
	}
	while (1)
	{
		if (psIndicator == 0)
		{
			if (close(descr[0]) == -1)
			{
                detectError("descriptor 0 closing failed");
                return 0;
			}
			if (dup2(descr[1], 1) == -1)
			{
                detectError("dup2 failed");
                return 0;
			}
			if (execl("./hello", "./hello", NULL) == -1)
			{
                detectError("execl failed");
                return 0;
			}
		}
		else
		{
            if (close(descr[1]) == -1)
			{
                detectError("descriptor 1 closing failed");
                return 0;
			}
			if(read(descr[0], hello, 7) != -1)
			{
				t = time(NULL);
				time_template = localtime(&t);
				if (time_template == NULL)
				{
                    detectError("time_template creation failed");
                    return 0;
				}
				if (strftime(outstring, sizeof(outstring), "%H:%M:%S", time_template) == 0)
				{
                    detectError("srtftime failed");
                    return 0;
				}
				printf("%s %s", outstring, hello);
			}
		}
	}
	return 0;
}
