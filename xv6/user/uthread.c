#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"


void lock_acquire(lock_t* lock)
{
  while(xchg(lock, 1) == 1);
}

void lock_release(lock_t* lock)
{
  xchg(lock, 0);
}

void lock_init(lock_t* lock)
{
  *lock = 0;
}

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
  return pid;
}