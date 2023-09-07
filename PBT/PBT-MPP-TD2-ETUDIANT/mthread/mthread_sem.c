#include "mthread_internal.h"

/* Functions for handling semaphore.  */

int mthread_sem_init(mthread_sem_t *sem, unsigned int value)
{
  not_implemented();
  return 0;
}

/* P(sem), wait(sem) */
int mthread_sem_wait(mthread_sem_t *sem)
{
  not_implemented();
  return 0;
}

/* V(sem), signal(sem) */
int mthread_sem_post(mthread_sem_t *sem)
{
  not_implemented();
  return 0;
}

int mthread_sem_getvalue(mthread_sem_t *sem, int *sval)
{
  not_implemented();
  return 0;
}

int mthread_sem_trywait(mthread_sem_t *sem)
{
  not_implemented();
  return 0;
}

/* undo sem_init() */
int mthread_sem_destroy(mthread_sem_t *sem)
{
  not_implemented();
  return 0;
}
