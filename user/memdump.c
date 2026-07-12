#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void memdump(char *fmt, char *data);

int
main(int argc, char *argv[])
{
  if(argc == 1){
    printf("Example 1:\n");
    int a[2] = { 61810, 2025 };
    memdump("ii", (char*) a);
    
    printf("Example 2:\n");
    memdump("S", "a string");
    
    printf("Example 3:\n");
    char *s = "another";
    memdump("s", (char *) &s);

    struct sss {
      char *ptr;
      int num1;
      short num2;
      char byte;
      char bytes[8];
    } example;
    
    example.ptr = "hello";
    example.num1 = 1819438967;
    example.num2 = 100;
    example.byte = 'z';
    strcpy(example.bytes, "xyzzy");
    
    printf("Example 4:\n");
    memdump("pihcS", (char*) &example);
    
    printf("Example 5:\n");
    memdump("sccccc", (char*) &example);
  } else if(argc == 2){
    // format in argv[1], up to 512 bytes of data from standard input.
    char data[512];
    int n = 0;
    memset(data, '\0', sizeof(data));
    while(n < sizeof(data)){
      int nn = read(0, data + n, sizeof(data) - n);
      if(nn <= 0)
        break;
      n += nn;
    }
    memdump(argv[1], data);
  } else {
    printf("Usage: memdump [format]\n");
    exit(1);
  }
  exit(0);
}

// i：将接下来的 4 字节数据打印为 32 位十进制整数。
// p：将接下来的 8 字节数据打印为 64 位十六进制整数。
// h：将接下来的 2 字节数据打印为 16 位十进制整数。
// c：将接下来的 1 字节数据打印为 8 位 ASCII 字符。
// s：接下来的 8 字节数据包含指向 C 字符串的 64 位指针；打印该字符串。
// S：数据的其余部分包含以空字符结尾的 C 字符串的字节；打印该字符串。

void
memdump(char *fmt, char *data)
{
  // Your code here.

  int i;
  for (i = 0; fmt[i] != '\0'; i++)
  {
	if (fmt[i] == 'i')
	{
		int *cur = (int *)data;
		data += 4;
		printf("%d", *cur);
	}
	else if (fmt[i] == 'p')
	{
		long long *cur = (long long *)data;
		data += 8;
		printf("%llx", *cur);
	}
	else if (fmt[i] == 'h')
	{
		short *cur = (short *)data;
		data += 2;
		printf("%d", *cur);
	}
	else if (fmt[i] == 'c')
	{
		printf("%c", *data);
		data++;
	}
	else if (fmt[i] == 's')
	{
		char **cur = (char **)data;
		data += 8;
		printf("%s", *cur);
	}
	else if (fmt[i] == 'S')
	{
		printf("%s", data);
		data += strlen(data) + 1;
	}
	printf("\n");
  }
}
