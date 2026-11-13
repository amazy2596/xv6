#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

int matchhere(char*, char*);
int matchstar(int, char*, char*);

int
match(char *re, char *text)
{
  if(re[0] == '^')
    return matchhere(re+1, text);
  do{  // must look at empty string
    if(matchhere(re, text))
      return 1;
  }while(*text++ != '\0');
  return 0;
}

// matchhere: search for re at beginning of text
int matchhere(char *re, char *text)
{
  if(re[0] == '\0')
    return 1;
  if(re[1] == '*')
    return matchstar(re[0], re+2, text);
  if(re[0] == '$' && re[1] == '\0')
    return *text == '\0';
  if(*text!='\0' && (re[0]=='.' || re[0]==*text))
    return matchhere(re+1, text+1);
  return 0;
}

// matchstar: search for c*re at beginning of text
int matchstar(int c, char *re, char *text)
{
  do{  // a * matches zero or more instances
    if(matchhere(re, text))
      return 1;
  }while(*text!='\0' && (*text++==c || c=='.'));
  return 0;
}

char *myargv[MAXARG];
int is_execute = 0;

void debug(char *msg, char *s)
{
	printf("%s: %s\n", msg, s);
}

char* get_name(char *path)
{
	char *p;
	for (p = path + strlen(path); *p != '/' && p >= path; p--);
	p++;

	return p;
}

void execute(char *path)
{
	myargv[is_execute++] = path;
	myargv[is_execute] = 0;

	// for (int i = 0; myargv[i] != 0; i++)
	// {
		// printf("%s ", myargv[i]);
	// }
	// printf("\n");

	if (fork() == 0)
	{
		exec(myargv[0], myargv);
		exit(0);
	}
	else 
	{
		myargv[--is_execute] = 0;
		wait(0);
		return;
	}
}

void find(char *path, char *re)
{
	char buf[512], *p, *tmp;
	int fd;
	struct dirent de;
	struct stat st;

	if ((fd = open(path, O_RDONLY)) < 0)
	{
		fprintf(2, "find: cannot open %s\n", path);
		return;
	}

	if (fstat(fd, &st) < 0)
	{
		fprintf(2, "find, cannot stat %s\n", path);
		close(fd);
		return;
	}

	if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf))
	{
		fprintf(2, "find: path too long\n");
		return;
	}

	strcpy(buf, path);
	p = buf + strlen(buf);
	*(p++) = '/';

	while (read(fd, &de, sizeof(de)) == sizeof(de))
	{
		if (de.inum == 0) continue;
		memmove(p, de.name, DIRSIZ);
		p[DIRSIZ] = '\0';

		if (stat(buf, &st) < 0)
		{
			fprintf(2, "find: cannot stat\n");
			break;
		}

		tmp = get_name(buf);

		if (st.type == T_FILE && match(re, tmp))
		{
			if (is_execute)
			{
				execute(buf);
			}
			else 
			{
				printf("%s\n", buf);
			}
			continue;
		}

		if (!strcmp(tmp, ".") || !strcmp(tmp, "..") || st.type != T_DIR) 
		{
			continue;
		}

		find(buf, re);
	}
	close(fd);
}

int main(int argc, char *argv[])
{
	if (argc < 3)
	{
		fprintf(2, "usage miss arg\n");
		exit(1);
	}

	if (argc == 3)
	{
		find(argv[1], argv[2]);
	}
	else 
	{
		if (argc - 1 >= MAXARG)
		{
			fprintf(2, "usage too many arg\n");
			exit(1);
		}
		for (is_execute = 0; argv[is_execute + 4] != 0; is_execute++) myargv[is_execute] = argv[is_execute + 4];
		find(argv[1], argv[2]);
	}
	exit(0);
}