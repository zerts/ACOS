#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

extern int errno;

void detectError(const char * err)
{
    perror(err);
    errno = 0;
}

void printInCurrLength(char * out, int len)
{
    printf("%s", out);
    for (int i = 0; i < len - strlen(out); i++)
        printf(" ");
}

void write_full_information(struct dirent * currdir, char * path)
{
    struct stat information;
    char * s = malloc(strlen(currdir->d_name) + strlen(path) + 10);
    if (s == NULL)
    {
        detectError("malloc in write_full_information failed");
        return;
    }
	memcpy(s, path, strlen(path));
	if (s == NULL)
    {
        detectError("memcpy in write_full_information failed");
        return;
    }
	int len = strlen(path);
 	memcpy(s + len, currdir->d_name, strlen(currdir->d_name));
 	if (s == NULL)
    {
        detectError("memcpy in write_full_information failed");
        return;
    }
 	len += strlen(currdir->d_name);
	memcpy(s + len, "\0", 1);
	if (s == NULL)
    {
        detectError("memcpy in write_full_information failed");
        return;
    }
    if (stat(s, &information) == -1)
    {
        detectError("Bad file or directory");
        return;
    }
    free(s);
    int uw = 0, ur = 0, ue = 0, gw = 0, gr = 0, ge = 0, ow = 0, orrr = 0, oe = 0;

    switch (information.st_mode & S_IFMT)
    {
       case S_IFBLK: break;
       case S_IFCHR: break;
       case S_IFDIR: printf("d"); break;
       default: printf("-"); break;
    }
    switch (information.st_mode & S_IRWXU)
    {
        case S_IRUSR: ur = 1; break;
        case S_IWUSR: uw = 1; break;
        case S_IXUSR: ue = 1; break;
        case S_IRUSR + S_IWUSR: ur = 1; uw = 1; break;
        case S_IRUSR + S_IXUSR: ur = 1; ue = 1; break;
        case S_IXUSR + S_IWUSR: ue = 1; uw = 1; break;
        case S_IRUSR + S_IWUSR + S_IXUSR: ur = 1; uw = 1; ue = 1; break;
        default: detectError("getting read/write information failed"); return;
    }
    switch (information.st_mode & S_IRWXG)
    {
        case S_IRGRP: gr = 1; break;
        case S_IWGRP: gw = 1; break;
        case S_IXGRP: ge = 1; break;
        case S_IRGRP + S_IWGRP: gr = 1; gw = 1; break;
        case S_IRGRP + S_IXGRP: gr = 1; ge = 1; break;
        case S_IXGRP + S_IWGRP: ge = 1; gw = 1; break;
        case S_IRGRP + S_IWGRP + S_IXGRP: gr = 1; gw = 1; ge = 1; break;
        default: detectError("getting read/write information failed"); return;
    }
    switch (information.st_mode & S_IRWXO)
    {
        case S_IROTH: orrr = 1; break;
        case S_IWOTH: ow = 1; break;
        case S_IXOTH: oe = 1; break;
        case S_IROTH + S_IWOTH: orrr = 1; ow = 1; break;
        case S_IROTH + S_IXOTH: orrr = 1; oe = 1; break;
        case S_IWOTH + S_IXOTH: ow = 1; oe = 1; break;
        case S_IROTH + S_IWOTH + S_IXOTH: orrr = 1; ow = 1; oe = 1; break;
        default: detectError("getting read/write information failed"); return;
    }

    if (ur == 1)
        printf ("r");
    else
        printf ("-");
    if (uw == 1)
        printf ("w");
    else
        printf ("-");
    if (ue == 1)
        printf ("x");
    else
        printf ("-");

    if (gr == 1)
        printf ("r");
    else
        printf ("-");
    if (gw == 1)
        printf ("w");
    else
        printf ("-");
    if (ge == 1)
        printf ("x");
    else
        printf ("-");

    if (orrr == 1)
        printf ("r");
    else
        printf ("-");
    if (ow == 1)
        printf ("w");
    else
        printf ("-");
    if (oe == 1)
        printf ("x");
    else
        printf ("-");

    printf(" ");
    printf("% 4d ", (int)information.st_nlink);
    printf("% 10d ", (int)information.st_size);
    struct passwd * user = getpwuid(information.st_uid);
    struct group * groupName = getgrgid(information.st_gid);
    if (user == NULL)
    {
        detectError("No user");
        return;
    }
    if (groupName == NULL)
    {
        detectError("No group");
        return;
    }
    printInCurrLength(user->pw_name, 10);
    printInCurrLength(groupName->gr_name, 10);
    char outtime[10];
    time_t t = information.st_mtime;
    struct tm *tmp;
    tmp = localtime(&t);
    if (tmp == NULL)
    {
        detectError("getting time failed");
        return;
    }
    strftime(outtime, sizeof(outtime), "%B", tmp);
    printInCurrLength(outtime, 10);
    strftime(outtime, sizeof(outtime), "%H:%M:%S", tmp);
    printInCurrLength(outtime, 10);

    printf("%s\n", currdir->d_name);

}

void write_child(struct dirent * _dir, const char * const way, int tab, int mode_l)
{
	struct dirent *currdir;
	DIR *dir;
	char * s = malloc(strlen(_dir->d_name) + strlen(way) + 3);
	if (s == NULL)
    {
        detectError("malloc in write_child failed");
        return;
    }
	memcpy(s, way, strlen(way));
	if (s == NULL)
    {
        detectError("memcpy in write_child failed");
        return;
    }
	int len = strlen(way);
	memcpy(s + len, _dir->d_name, strlen(_dir->d_name));
	if (s == NULL)
    {
        detectError("memcpy in write_child failed");
        return;
    }
	len += strlen(_dir->d_name);
	memcpy(s + len, "/\0", 2);
	if (s == NULL)
    {
        detectError("memcpy in write_child failed");
        return;
    }
	if	(dir = opendir(s))
	{
        if (dir == NULL)
        {
            detectError("opendir in write_child failed");
            return;
        }
        while (currdir = readdir(dir))
        {
            if (currdir == NULL)
            {
                detectError("readdir int write_child failed");
                return;
            }
            if (currdir->d_name[0] != '.')
            {
                for (int i = 0; i < tab; i++)
                        fprintf(stdout, "-");
                if (mode_l == 0)
                {
                    fprintf(stdout, "%s\n", currdir->d_name);
                }
                else
                {
                    printf("|");
                    fflush(stdout);
                    write_full_information(currdir, s);
                }
                write_child(currdir, s, tab + 5, mode_l);
            }
        }
	}
	free(s);
	closedir(dir);
	return;
}

int main(int argc, char** argv)
{
    int mode_l = 0, mode_R = 0, mode_addr = 0;
    int opt, numberOfOptions = 0;
    while ((opt = getopt(argc, argv, "lR")) != -1)
    {
        numberOfOptions++;
        switch (opt)
		{
			case 'l':
				mode_l = 1;
				break;
			case 'R':
				mode_R = 1;
				break;
			default:
				break;
		}
    }
    if (argc == numberOfOptions + 2)
    {
        mode_addr = 1;
    }
    else if (argc > numberOfOptions + 2)
    {
        detectError("bad number of arguments");
        return 0;
    }
	DIR * dir;
	struct dirent * child;
	char * base_addr, * empty_string = "";
	if (mode_addr == 1)
        base_addr = argv[argc - 1];
    else
        base_addr = "./";
    dir = opendir(base_addr);
	if (dir == NULL && errno != 0)
	{
        detectError("Bad base dirrectory address");
        return 0;
	}
	while (child = readdir(dir))
	{
        if (child == NULL)
        {
            detectError("readdir in main failed");
            return 0;
        }
		if (child->d_name[0] != '.')
		{
            if (mode_l == 0 && mode_R == 0)
            {
                fprintf(stdout, "%s\n", child->d_name);
            }
            else if (mode_l == 1 && mode_R == 0)
            {
                write_full_information(child, empty_string);
            }
            else if (mode_R == 1)
            {
                if (mode_l == 0)
                {
                    fprintf(stdout, "%s\n", child->d_name);
                }
                else
                {
                    write_full_information(child, empty_string);
                }
                write_child(child, base_addr, 5, mode_l);
            }
		}
	}
	if (closedir(dir) == -1)
	{
        detectError("main closedir failed");
        return 0;
	}
	return 0;
}
