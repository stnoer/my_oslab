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

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run
{
  struct run *next;
};

struct kmem
{
  struct spinlock lock;
  struct run *freelist;
};

struct kmem kmems[NCPU]; // 为每个CPU分配独立的freelist

void kinit()
{
  char lockname[20];
  for (int i = 0; i < NCPU; i++)
  {
    snprintf(lockname, sizeof(lockname), "kmem%d", i);
    initlock(&(kmems[i].lock), "kmem");
  }
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 112, PGSIZE);

  r = (struct run *)pa;

  push_off();

  int cpu = cpuid();
  acquire(&kmems[cpu].lock);
  r->next = kmems[cpu].freelist;
  kmems[cpu].freelist = r;
  release(&kmems[cpu].lock);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r = 0;

  push_off();

  int cpu = cpuid();

  acquire(&kmems[cpu].lock);
  r = kmems[cpu].freelist;
  if (r)
    kmems[cpu].freelist = r->next;
  release(&kmems[cpu].lock);

  if (!r)
  {
    for (int i = (cpu + 1) % NCPU; i != cpu; i = (i + 1) % NCPU)
    {
      acquire(&kmems[i].lock);
      r = kmems[i].freelist;
      if (r)
      {
        kmems[i].freelist = r->next;
        release(&kmems[i].lock);
        break;
      }
      release(&kmems[i].lock);
    }
  }

  pop_off();

  if (r)
  {
    memset((char *)r, 5, PGSIZE); // fill with junk
    return (void *)r;
  }
  else
    return 0;
}
