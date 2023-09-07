#include "mthread_internal.h"
#include <errno.h>

/* Functions for handling semaphore.  */

//Q2
int mthread_sem_init(mthread_sem_t *sem, unsigned int value)
{
  if (sem == NULL || value == 0) {
    return EINVAL;
  }

  sem->lock = 0;
  sem->value = value;
  sem->max = value;

  return 0;
}

/* P(sem), wait(sem) */
//Q3
int mthread_sem_wait(mthread_sem_t *sem)
{
  if (sem == NULL)
    return EINVAL;

  mthread_spinlock_lock(&sem->lock);

  if (sem->value > 0) {
    sem->value--;
  } else {
    while (sem->value == 0)
      mthread_yield();
    sem->value--;
  }
  mthread_spinlock_unlock(&sem->lock);

  return 0;
}

/* V(sem), signal(sem) */
//Q4
int mthread_sem_post(mthread_sem_t *sem)
{
  if (sem == NULL)
    return EINVAL;

  mthread_spinlock_lock(&sem->lock);
  sem->value++;
  mthread_spinlock_unlock(&sem->lock);
  return 0;
}

/* undo sem_init() */
//Q5
int mthread_sem_destroy(mthread_sem_t *sem)
{
  if (sem == NULL)
    return EINVAL;

  mthread_spinlock_lock(&sem->lock);
  if (sem->value != sem->max) {
     mthread_spinlock_unlock(&sem->lock);
    return EBUSY;
  }

  mthread_spinlock_unlock(&sem->lock);

  return 0;
}

//Q6
int mthread_sem_trywait(mthread_sem_t *sem)
{
  if (sem == NULL)
    return EINVAL;

  if (sem->value == 0)
    return EBUSY;

  return mthread_sem_wait(sem);
}

//Q7
int mthread_sem_getvalue(mthread_sem_t *sem, int *sval)
{
  if (sem == NULL || sval == NULL)
    return EINVAL;

  mthread_spinlock_lock(&sem->lock);
  *sval = sem->value;
  mthread_spinlock_unlock(&sem->lock);

  return 0;
}

