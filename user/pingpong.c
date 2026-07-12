#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
	int a[2], b[2];
	pipe(a), pipe(b);

	int n = 10000;
	if (argc > 1) n = atoi(argv[1]);
	int start = uptime();
	if (fork() == 0)
	{
		close(a[1]);
		close(b[0]);
		char buf;
		for (int i = 0; i < n; i++)
		{
			if (read(a[0], &buf, 1) == 1) 
				write(b[1], &buf, 1);
		}
		exit(0);
	}
	else 
	{
		close(a[0]);
		close(b[1]);
		write(a[1], "h", 1);
		char buf;
		for (int i = 0; i < n; i++)
		{
			if (read(b[0], &buf, 1) == 1) 
			{
				write(a[1], &buf, 1);
			}
		}

		int end = uptime();
		if (end - start == 0) printf("too short\n");
		else printf("the speed is %d/s\n", n * 10 / (end - start));
		wait(0);
	}

	exit(0);
}