#include <errno.h>

#include "mthread_internal.h"

/* Functions for handling conditional variables.  */

int __mthread_cond_unchecked_ensure_thread_queue_init(mthread_cond_t *cond)
{
  if (cond->thread_queue != NULL || cond->lock != 0)
    return EINVAL;

  mthread_log("COND ENSURE THREAD", "Initializing\n");

  cond->thread_queue = malloc(sizeof(mthread_list_t));
  if (cond->thread_queue == NULL)
  {
    perror("malloc for cond internal thread queue");
    exit(errno);
  }

  *(cond->thread_queue) = (mthread_list_t)MTHREAD_LIST_INIT;

  mthread_log("COND ENSURE THREAD", "Initialized\n");
  return 0;
}

/* Initialize condition variable COND using attributes ATTR, or use
     the default values if later is NULL.  */
int mthread_cond_init(mthread_cond_t *cond, const mthread_condattr_t *cond_attr)
{
  mthread_log("COND INIT", "Initializing\n");

  if (cond == NULL)
  {
    mthread_log("COND INIT", "Returning EINVAL\n");
    return EINVAL;
  }

  cond->lock = 0;

  __mthread_cond_unchecked_ensure_thread_queue_init(cond);

  mthread_log("COND INIT", "Initialized\n");
  return 0;
}

/* Wait for condition variable COND to be signaled or broadcast.
     MUTEX is assumed to be locked before.  */
int mthread_cond_wait(mthread_cond_t *cond, mthread_mutex_t *mutex)
{
  mthread_log("COND WAIT", "Waiting\n");
  if (cond == NULL || mutex == NULL)
  {
    mthread_log("COND WAIT", "Arg was NULL\n");
    return EINVAL;
  }

  __mthread_cond_unchecked_ensure_thread_queue_init(cond);

  mthread_spinlock_lock(&cond->lock);

  mthread_virtual_processor_t *vp = mthread_get_vp();

  // Added: saving the previous state, to ensure the rollback is possible if necessary
  volatile struct mthread_s *prev_last = cond->thread_queue->last;
  mthread_tst_t *prev_lock = vp->p;

  // Added: blocking the current thread
  mthread_t self = mthread_self();
  self->status = BLOCKED;
  vp->p = &cond->lock;

  // Added: inserting the current thread at the end of the waiting list for the cond
  mthread_insert_last(self, cond->thread_queue);

  // Added: unlocking the mutex and, in case there was an error, rollback the changes
  int err = mthread_mutex_unlock(mutex);
  if (err != 0)
  {
    self->status = RUNNING;
    cond->thread_queue->last = prev_last;
    prev_last->next = NULL;
    vp->p = prev_lock;
    mthread_spinlock_unlock(&cond->lock);
    mthread_log("COND WAIT", "Error unlocking mutex\n");
    return err;
  }

  mthread_spinlock_unlock(&cond->lock);

  mthread_yield();

  // Added: relocking the mutex before exiting the function, returning the result
  mthread_log("COND WAIT", "Waited\n");
  return mthread_mutex_lock(mutex);
}

/* Wake up one thread waiting for condition variable COND.  */
int mthread_cond_signal(mthread_cond_t *cond)
{
  mthread_log("COND SIGNAL", "Signaling\n");
  if (cond == NULL)
  {
    mthread_log("COND SIGNAL", "Returning EINVAL\n");
    return EINVAL;
  }

  __mthread_cond_unchecked_ensure_thread_queue_init(cond);

  mthread_spinlock_lock(&cond->lock);

  // Added: get the first thread to wait, which is also the first we will signal
  struct mthread_s *th = mthread_remove_first(cond->thread_queue);

  // Added: ensure there is at least one waiting thread
  if (th == NULL)
  {
    mthread_spinlock_unlock(&cond->lock);
    mthread_log("COND SIGNAL", "No waiting thread\n");
    return EINVAL;
  }

  mthread_virtual_processor_t *vp = mthread_get_vp();
  th->status = RUNNING;
  mthread_insert_last(th, &(vp->ready_list));

  mthread_spinlock_unlock(&cond->lock);

  mthread_log("COND SIGNAL", "Signaled\n");
  return 0;
}

/* Wake up all threads waiting for condition variables COND.  */
int mthread_cond_broadcast(mthread_cond_t *cond)
{
  mthread_log("COND BROADCAST", "Broadcasting\n");
  if (cond == NULL)
  {
    mthread_log("COND BROADCAST", "Returning EINVAL\n");
    return EINVAL;
  }

  __mthread_cond_unchecked_ensure_thread_queue_init(cond);

  mthread_spinlock_lock(&cond->lock);

  // Added: get all the threads, one after the other, and set them to running.
  mthread_virtual_processor_t *vp = mthread_get_vp();
  struct mthread_s *th = NULL;
  while ((th = mthread_remove_first(cond->thread_queue)) != NULL)
  {
    th->status = RUNNING;
    mthread_insert_last(th, &(vp->ready_list));
  }

  mthread_spinlock_unlock(&cond->lock);

  mthread_log("COND BROADCAST", "Broadcasted\n");
  return 0;
}

/* Destroy condition variable COND.  */
int mthread_cond_destroy(mthread_cond_t *cond)
{
  mthread_log("COND DESTROY", "Destroying\n");

  if (cond == NULL)
  {
    mthread_log("COND DESTROY", "Returning EINVAL\n");
    return EINVAL;
  }

  mthread_spinlock_lock(&cond->lock);

  if (cond->thread_queue != NULL && cond->thread_queue->first != NULL)
  {
    mthread_spinlock_unlock(&cond->lock);
    mthread_log("COND DESTROY", "Returning EBUSY\n");
    return EBUSY;
  }

  // The queue is free but not it's members: they may be used elsewhere,
  // for another conditions for example
  if (cond->thread_queue != NULL)
  {
    free(cond->thread_queue);
    cond->thread_queue = NULL;
  }

  mthread_spinlock_unlock(&cond->lock);

  mthread_log("COND DESTROY", "Destroying\n");
  return 0;
}