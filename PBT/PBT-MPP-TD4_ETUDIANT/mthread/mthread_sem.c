#include <errno.h>

#include "mthread_internal.h"

/* Functions for handling semaphore.  */

int __mthread_sem_unchecked_ensure_thread_queue_init(mthread_sem_t *sem)
{
  if (sem->thread_queue != NULL || sem->lock != 0)
    return EINVAL;

  mthread_log("SEM ENSURE THREAD", "Initializing\n");

  sem->thread_queue = malloc(sizeof(mthread_list_t));
  if (sem->thread_queue == NULL)
  {
    perror("malloc for sem internal thread queue");
    exit(errno);
  }

  *(sem->thread_queue) = (mthread_list_t)MTHREAD_LIST_INIT;

  mthread_log("SEM ENSURE THREAD", "Initialized\n");
  return 0;
}

int mthread_sem_init(mthread_sem_t *sem, unsigned int value)
{
  mthread_log("SEM INIT", "Initializing\n");

  if (sem == NULL || value == 0)
  {
    mthread_log("SEM INIT", "Returning EINVAL\n");
    return EINVAL;
  }

  sem->max = value;
  sem->value = value;
  sem->lock = 0;

  __mthread_sem_unchecked_ensure_thread_queue_init(sem);

  mthread_log("SEM INIT", "Initialized\n");
  return 0;
}

/* undo sem_init() */
int mthread_sem_destroy(mthread_sem_t *sem)
{
  mthread_log("SEM DESTROY", "Destroying\n");
  if (sem == NULL)
  {
    mthread_log("SEM DESTROY", "Sem was NULL\n");
    return EINVAL;
  }

  mthread_spinlock_lock(&sem->lock);

  if (sem->value != sem->max)
  {
    mthread_spinlock_unlock(&sem->lock);
    mthread_log("SEM DESTROY", "Returning EBUSY\n");
    return EBUSY;
  }

  if (sem->thread_queue != NULL)
  {
    // The queue is free but not it's members: they may be used elsewhere,
    // for another semaphore for example
    free(sem->thread_queue);
    sem->thread_queue = NULL;
  }
  mthread_spinlock_unlock(&sem->lock);

  mthread_log("SEM DESTROY", "Destroyed\n");
  return 0;
}

int mthread_sem_trywait(mthread_sem_t *sem)
{
  mthread_log("SEM TRYWAIT", "Waiting\n");

  if (sem == NULL)
  {
    mthread_log("SEM TRYWAIT", "Sem was NULL\n");
    return EINVAL;
  }

  __mthread_sem_unchecked_ensure_thread_queue_init(sem);

  mthread_spinlock_lock(&sem->lock);

  if (sem->value == 0)
  {
    mthread_spinlock_unlock(&sem->lock);
    mthread_log("SEM TRYWAIT", "Sem was full, EBUSY returned\n");
    return EBUSY;
  }

  sem->value--;
  mthread_spinlock_unlock(&sem->lock);

  mthread_log("SEM TRYWAIT", "Managed to acquire semaphore\n");
  return 0;
}

/* P(sem), wait(sem) */
int mthread_sem_wait(mthread_sem_t *sem)
{
  mthread_log("SEM WAIT", "Waiting\n");

  if (sem == NULL)
  {
    mthread_log("SEM WAIT", "Sem was NULL\n");
    return EINVAL;
  }

  __mthread_sem_unchecked_ensure_thread_queue_init(sem);

  mthread_spinlock_lock(&sem->lock);

  if (sem->value > 0)
  {
    sem->value--;
    mthread_spinlock_unlock(&sem->lock);
  }
  else
  {
    // See mthread_sem_post for why value is not modified here
    mthread_t self = mthread_self();
    mthread_insert_last(self, sem->thread_queue);
    self->status = BLOCKED;
    mthread_virtual_processor_t *vp = mthread_get_vp();
    vp->p = &sem->lock;
    mthread_spinlock_unlock(&sem->lock);
    mthread_yield();
  }

  mthread_log("SEM WAIT", "Waited\n");
  return 0;
}

/* V(sem), signal(sem) */
int mthread_sem_post(mthread_sem_t *sem)
{
  mthread_log("SEM POST", "Posting\n");

  if (sem == NULL)
  {
    mthread_log("SEM POST", "Sem was NULL\n");
    return EINVAL;
  }

  __mthread_sem_unchecked_ensure_thread_queue_init(sem);

  mthread_spinlock_lock(&sem->lock);

  if (sem->thread_queue->first != NULL)
  {
    // No need to modify value here: the current thread is replaced by 'first',
    // meaning value stays to its current number
    mthread_t first = mthread_remove_first(sem->thread_queue);
    mthread_virtual_processor_t *vp = mthread_get_vp();
    first->status = RUNNING;
    mthread_insert_last(first, &(vp->ready_list));
  }
  else
  {
    sem->value++;
    sem->value = sem->value > sem->max ? sem->max : sem->value;
  }

  mthread_spinlock_unlock(&sem->lock);

  mthread_log("SEM POST", "Posted\n");
  return 0;
}

int mthread_sem_getvalue(mthread_sem_t *sem, int *sval)
{
  mthread_log("SEM GETVALUE", "Getting value\n");
  if (sem == NULL || sval == NULL)
  {
    mthread_log("SEM GETVALUE", "Arg was NULL\n");
    return EINVAL;
  }

  mthread_spinlock_lock(&sem->lock);
  *sval = sem->value;
  mthread_spinlock_unlock(&sem->lock);

  mthread_log("SEM GETVALUE", "Got value\n");
  return 0;
}
