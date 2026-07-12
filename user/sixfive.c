#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int is_number(char *s)
{
	if (s[0] == '\0') 
		return 0;

	for (int i = 0; s[i] != '\0'; i++)
	{
		if (s[i] < '0' || s[i] > '9')
		{
			return 0;
		}
	}

	return 1;
}

void process(char *buf, int *len)
{
	if (!is_number(buf)) 
	{
		*len = 0;
		return;
	}
	int x = atoi(buf);
	if (x % 5 == 0 ||  x % 6 == 0) printf("%d\n", x);
	*len = 0;
	return;
}

int main(int argc, char *argv[])
{
	if (argc == 1)
	{
		fprintf(2, "error\n");
		exit(1);
	}

	for (int i = 1; i < argc; i++)
	{
		int fd = open(argv[i], O_RDONLY);
		char buf[512];
		char c;
		int len = 0;
		while (read(fd, &c, 1) > 0)
		{
			if (strchr(" -\r\t\n./,", c))
			{
				process(buf, &len);
			}
			else 
			{
				buf[len++] = c;
				buf[len] = '\0';
			}
		}

		if (len > 0) process(buf, &len);
	}

	exit(0);
}