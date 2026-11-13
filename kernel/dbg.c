#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"

int
dbgwrite(int user_src, uint64 src, int n)
{
  char cmd;

  if(n < 1)
    return 0;
  if(either_copyin(&cmd, user_src, src, 1) < 0)
    return -1;
  if(cmd == '\n')          // echo 的第二次 write，直接吞掉
    return n;

  switch(cmd){
  case 'p':                // 打印当前进程
    printf("dbg: pid=%d name=%s\n", myproc()->pid, myproc()->name);
    break;
  case 'k':                // 分配一页物理内存，看地址
    printf("dbg: kalloc=%p\n", kalloc());
    break;
  default:
    printf("dbg: unknown cmd=%d\n", cmd);
    break;
  }
  return n;
}

void
dbginit(void)
{
  devsw[DBG].write = dbgwrite;
}