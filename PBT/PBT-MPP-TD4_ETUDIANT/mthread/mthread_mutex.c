#include <errno.h>
#include "mthread_internal.h"

/* Functions for mutex handling.  */

int __mthread_mutex_unchecked_ensure_list_init(mthread_mutex_t *mutex)
{
  if (mutex->list != NULL || mutex->lock != 0)
    return EINVAL;

  mthread_log("MUTEX ENSURE LIST", "Initializing\n");

  mutex->list = malloc(sizeof(mthread_list_t));
  if (mutex->list == NULL)
  {
    perror("malloc for mutex internal thread list");
    exit(errno);
  }

  *(mutex->list) = (mthread_list_t)MTHREAD_LIST_INIT;

  mthread_log("MUTEX ENSURE LIST", "Initialized\n");
  return 0;
}

/* Initialize MUTEX using attributes in *MUTEX_ATTR, or use the
   default values if later is NULL.  */
int mthread_mutex_init(mthread_mutex_t *mutex, const mthread_mutexattr_t *mutex_attr)
{
  mthread_log("MUTEX INIT", "Initializing\n");

  if (mutex == NULL)
  {
    mthread_log("MUTEX INIT", "Mutex was NULL\n");
    return EINVAL;
  }

  // Added: The mutex is not locked by default
  mutex->nb_thread = 0;
  mutex->lock = 0;

  // Added: ensure list is initialized
  __mthread_mutex_unchecked_ensure_list_init(mutex);

  mthread_log("MUTEX INIT", "Initialized\n");
  return 0;
}

// Destroy MUTEX.
int mthread_mutex_destroy(mthread_mutex_t *mutex)
{
  mthread_log("MUTEX DESTROY", "Destroying\n");
  if (mutex == NULL)
  {
    mthread_log("MUTEX DESTROY", "Mutex was NULL\n");
    return EINVAL;
  }

  mthread_spinlock_lock(&mutex->lock);
  // Cannot free a busy mthread_mutex_t
  if (mutex->nb_thread != 0)
  {
    mthread_spinlock_unlock(&mutex->lock);
    mthread_log("MUTEX DESTROY", "Returning EBUSY\n");
    return EBUSY;
  }

  if (mutex->list != NULL)
  {
    // The list is free but not it's members: they may be used elsewhere,
    // for another mutex for example
    free(mutex->list);
    mutex->list = NULL;
  }
  mthread_spinlock_unlock(&mutex->lock);

  mthread_log("MUTEX DESTROY", "Destroyed\n");
  return 0;
}

// Try to lock MUTEX.
int mthread_mutex_trylock(mthread_mutex_t *mutex)
{
  mthread_log("MUTEX TRYLOCK", "Trying to lock\n");
  // Added: Test for valid mutex, test for busy mutex and finally, locking
  if (mutex == NULL)
  {
    mthread_log("MUTEX TRYLOCK", "Mutex was NULL\n");
    return EINVAL;
  }

  // Added: ensure list is initialized: this is for the static initializer
  __mthread_mutex_unchecked_ensure_list_init(mutex);

  mthread_spinlock_lock(&mutex->lock);

  if (mutex->nb_thread > 0)
  {
    mthread_spinlock_unlock(&mutex->lock);
    mthread_log("MUTEX TRYLOCK", "Already locked, returning EBUSY\n");
    return EBUSY;
  }

  mutex->nb_thread = 1;
  mthread_spinlock_unlock(&mutex->lock);

  mthread_log("MUTEX TRYLOCK", "Managed to lock\n");
  return 0;
}

// Wait until lock for MUTEX becomes available and lock it.
int mthread_mutex_lock(mthread_mutex_t *mutex)
{
  mthread_log("MUTEX LOCK", "Locking\n");
  // Added: deleted retval, moved self, moved vp
  if (mutex == NULL)
  {
    mthread_log("MUTEX LOCK", "Mutex was NULL\n");
    return EINVAL;
  }
  // Added: ensure list is initialized: this is for the static initializer
  __mthread_mutex_unchecked_ensure_list_init(mutex);

  mthread_spinlock_lock(&mutex->lock);

  if (mutex->nb_thread == 0)
  {
    mutex->nb_thread = 1;
    mthread_spinlock_unlock(&mutex->lock);
  }
  else
  {
    // See mthread_mutex_lock for why nb_thread is not modified here
    mthread_t self = mthread_self();
    mthread_insert_last(self, mutex->list);
    self->status = BLOCKED;
    mthread_virtual_processor_t *vp = mthread_get_vp();
    vp->p = &mutex->lock;
    mthread_spinlock_unlock(&mutex->lock);
    mthread_yield();
  }

  mthread_log("MUTEX LOCK", "Locked\n");
  return 0;
}

// Unlock MUTEX.
int mthread_mutex_unlock(mthread_mutex_t *mutex)
{
  mthread_log("MUTEX UNLOCK", "Unlocking\n");
  // Added: deleted retval, moved first, moved vp
  if (mutex == NULL)
  {
    mthread_log("MUTEX UNLOCK", "Mutex was NULL\n");
    return EINVAL;
  }
  // Added: ensure list is initialized: this is for the static initializer
  __mthread_mutex_unchecked_ensure_list_init(mutex);

  mthread_spinlock_lock(&mutex->lock);
  if (mutex->list->first != NULL)
  {
    // No need to modify nb_thread here: the current thread is replaced by 'first',
    // meaning nb_thread stays to 1
    mthread_t first = mthread_remove_first(mutex->list);
    mthread_virtual_processor_t *vp = mthread_get_vp();
    first->status = RUNNING;
    mthread_insert_last(first, &(vp->ready_list));
  }
  else
  {
    mutex->nb_thread = 0;
  }

  mthread_spinlock_unlock(&mutex->lock);

  mthread_log("MUTEX UNLOCK", "Unlocked\n");
  return 0;
}
