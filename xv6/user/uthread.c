#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"


int thread_create(void (*start_routine)(void*), void *arg)
{
  void* stack = NULL;
  int pid = -1;
  stack = malloc(2 * 4096);
  if ((uint)stack % 4096 != 0)
  {
    stack = stack + (4096 - (uint)stack % 4096);
  }
  if (stack == NULL)
    return pid;
  pid = clone(start_routine, arg, stack);
  return pid;
}

int thread_join()
{
  void* stack;
  int pid = 0;
  pid = join(&stack);
  if (stack == NULL)
  {
    return -1;
  }
  free(stack);
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