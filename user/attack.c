#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  // Your code here.
  char *p = sbrk(30 * 4096);
  char target[] = "This may help";
  strcpy(target+13, ".");
  while (memcmp(p, target, sizeof(target)) != 0 || *(p + 16) == '(')
    p++;
  printf("%s\n", p + 16);
  exit(1);
}
