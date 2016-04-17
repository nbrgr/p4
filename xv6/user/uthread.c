#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"


int thread_create(void (*start_routine)(void*), void *arg)
{
  void* stack = NULL;
  stack = malloc(4096);
  if (stack == NULL)
    return -1;
  return clone(start_routine, arg, stack);
}

int thread_join()
{
  void* stack;
  int pid = 0;
  pid = join(&stack);
  if (stack == NULL)
    return -1;
  free(stack);
  printf(1, "%d\n", pid);
  return pid;
}

void
lock_init(lock_t *lk)
{
  lk->locked = 0;
}

void
lock_acquire(lock_t *lk)
{  
  while(xchg(&lk->locked, 1) != 0)
    ;
}

void
lock_release(lock_t *lk)
{
  xchg(&lk->locked, 0);
}