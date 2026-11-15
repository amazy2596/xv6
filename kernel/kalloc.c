// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);
uint64 superfreerange();

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

#define NUM 20
struct {
  struct spinlock lock;
  struct run *superfreelist;
} superkmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&superkmem.lock, "superkmem");

  uint64 curstop = superfreerange();

  freerange(end, (void*)curstop);
  // freerange(end, (void*)PHYSTOP);

  // 测试
  // char *p[NUM];
  // for (int i = 0; i < NUM; i++)
  // {
  //   p[i] = superkalloc();
  //   printf("alloc addr %d: %ld\n", i, (uint64)p[i]);
  // }

  // for (int i = 0; i < NUM; i++)
  // {
  //   superkfree(p[i]);
  //   printf("free addr %d\n", i);
  // }

  // for (int i = 0; i < NUM; i++)
  // {
  //   p[i] = superkalloc();
  //   printf("realloc addr %d: %ld\n", i, (uint64)p[i]);
  // }
}

uint64 
superfreerange()
{
    char *p = (char*)PHYSTOP;
    char *s;
    for (int i = 0; i < NUM; i++)
    {
      s = (char*)SUPERPGROUNDDOWN((uint64)p);
      superkfree(s);
      p = s;
    }
    return (uint64)p;
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

void superkfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % SUPERPGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("superkfree");

  memset(pa, 1, SUPERPGSIZE);

  r = (struct run*)pa;

  acquire(&superkmem.lock);
  r->next = superkmem.superfreelist;
  superkmem.superfreelist = r;
  release(&superkmem.lock);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

void *
superkalloc(void)
{
  struct run *r;

  acquire(&superkmem.lock);
  r = superkmem.superfreelist;
  if (r)
    superkmem.superfreelist = r->next;
  release(&superkmem.lock);

  if (r)
    memset((char*)r, 5, SUPERPGSIZE);
  return (void*)r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
