// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKETS 13

struct
{
  struct spinlock lock;
  struct spinlock locks[NBUCKETS];
  struct buf buf[NBUF];
  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  // struct buf head;
  struct buf hashbucket[NBUCKETS]; // 每个哈希队列一个linked list及一个lock
} bcache;

void binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  char lockname[20];
  for (int i = 0; i < NBUCKETS; i++)
  {
    snprintf(lockname, sizeof(lockname), "bcachelock%d", i);
    initlock(&(bcache.locks[i]), lockname);
  }

  // Create linked list of buffers
  for (int i = 0; i < NBUCKETS; i++)
  {
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }

  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    b->next = bcache.hashbucket[0].next;
    b->prev = &bcache.hashbucket[0];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[0].next->prev = b;
    bcache.hashbucket[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b = 0;

  int num = blockno % NBUCKETS;

  acquire(&bcache.locks[num]);

  // Is the block already cached?
  for (b = bcache.hashbucket[num].next; b != &bcache.hashbucket[num]; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;
      release(&bcache.locks[num]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (b = bcache.hashbucket[num].prev; b != &bcache.hashbucket[num]; b = b->prev)
  {
    if (b->refcnt == 0)
    {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.locks[num]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached in num-bucket
  // select an unused cache from other buckets
  release(&bcache.locks[num]);

 
  //acquire(&bcache.locks[num]);
 // int count = 0;
  int i=(num + 1) % NBUCKETS;
  for ( ; i != num; i = (i + 1) % NBUCKETS)
  {
    acquire(&bcache.lock);
    acquire(&bcache.locks[i]);
    for (b = bcache.hashbucket[i].next; b != &bcache.hashbucket[i]; b = b->prev)
    {
      if (b->refcnt == 0)
      {
        
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        b->prev->next = b->next;
        b->next->prev = b->prev;

        
        acquire(&bcache.locks[num]);
        b->next = bcache.hashbucket[num].next;
        b->prev = &bcache.hashbucket[num];
        bcache.hashbucket[num].next->prev = b;
        bcache.hashbucket[num].next = b;

        
        release(&bcache.locks[num]);
        release(&bcache.locks[i]);
        release(&bcache.lock);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.locks[i]);
    release(&bcache.lock);
  }

  //release(&bcache.locks[num]);
  
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse"); 

  releasesleep(&b->lock);

  int num = b->blockno % NBUCKETS;
  acquire(&bcache.locks[num]);
  b->refcnt--;
  if (b->refcnt == 0)
  {
    // no one is waiting for it.
    
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[num].next;
    b->prev = &bcache.hashbucket[num];
    bcache.hashbucket[num].next->prev = b;
    bcache.hashbucket[num].next = b;
  }

  release(&bcache.locks[num]);
}

void bpin(struct buf *b)
{
  acquire(&bcache.locks[b->blockno % NBUCKETS]);
  b->refcnt++;
  release(&bcache.locks[b->blockno % NBUCKETS]);
}

void bunpin(struct buf *b)
{
  acquire(&bcache.locks[b->blockno % NBUCKETS]);
  b->refcnt--;
  release(&bcache.locks[b->blockno % NBUCKETS]);
}
