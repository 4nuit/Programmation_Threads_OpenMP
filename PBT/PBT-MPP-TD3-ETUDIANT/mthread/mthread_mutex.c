#include <errno.h>
#include "mthread_internal.h"

/* Functions for mutex handling.  */

// Added: a function to ensure list is correctly initialized, even after a MTHREAD_MUTEX_INITIALIZER (see 'mthread.h')

/// This function ensures the list member of the mutex is correctly built
/// It is a no-op when the mutex is locked or the list member is not null
void __mthread_ensure_list_init(mthread_mutex_t *__mutex)
{
  // If lock is one, it is assumed the mutex is already correctly built
  if (__mutex->lock == 1)
    return;

  // If list is not NULL, it is assumed the mutex is already correctly built
  if (__mutex->list != NULL)
    return;

  __mutex->list = malloc(sizeof(mthread_list_t));
  if (__mutex->list == NULL)
  {
    perror("malloc");
    exit(errno);
  }

  *(__mutex->list) = (mthread_list_t){ .first = NULL, .last = NULL, .lock = 0 };
}

/* Initialize MUTEX using attributes in *MUTEX_ATTR, or use the
   default values if later is NULL.  */
int mthread_mutex_init(mthread_mutex_t *__mutex, const mthread_mutexattr_t *__mutex_attr)
{
  if (__mutex == NULL)
    return EINVAL;

  if (__mutex_attr != NULL) {
    // Added: check of the given values
    if (// Type checks
        __mutex_attr->type < MTHREAD_MUTEX_NORMAL || __mutex_attr->type > MTHREAD_MUTEX_DEFAULT
        // Policy checks
        || (__mutex_attr->policy != MTHREAD_MUTEX_POLICY_FIRST_FIT && __mutex_attr->policy != MTHREAD_MUTEX_POLICY_FAIRSHARE)
      ) {
      return EINVAL;
    }
    __mutex->attr = *__mutex_attr;
  } else {
    // Added: default values for attributes
    __mutex->attr.type = MTHREAD_MUTEX_DEFAULT;
    __mutex->attr.protocol = 0;
    __mutex->attr.prioceiling = 0;
    __mutex->attr.policy = MTHREAD_MUTEX_POLICY_FIRST_FIT;
  }

  // Added: The mutex is not locked by default
  __mutex->nb_thread = 0;
  __mutex->lock = 0;

  // Added: replaced list init by call to specialized function
  __mthread_ensure_list_init(__mutex);

  fprintf(stderr, "[MUTEX_INIT] MUTEX initialized\n");

  return 0;
}

// Destroy MUTEX.
int mthread_mutex_destroy(mthread_mutex_t *__mutex)
{
  if (__mutex == NULL)
    // Added: Invalid value provided
    return EINVAL;

  // Added: ensure list is correct
  __mthread_ensure_list_init(__mutex);

  mthread_spinlock_lock(&__mutex->lock);
  // Cannot free a busy mthread_mutex_t
  if (__mutex->nb_thread != 0) {
    mthread_spinlock_unlock(&__mutex->lock);
    return EBUSY;
  }

  free(__mutex->list);
  __mutex->list = NULL;
  mthread_spinlock_unlock(&__mutex->lock);

  mthread_log("MUTEX_INIT", "MUTEX destroyed\n");
  return 0;
}

// Try to lock MUTEX.
int mthread_mutex_trylock(mthread_mutex_t *__mutex)
{
  // Added: Test for valid mutex, test for busy mutex and finally, locking
  if (__mutex == NULL)
    return EINVAL;

  if (__mutex->nb_thread >= 1)
    return EBUSY;

  // Added: ensure list is correct
  __mthread_ensure_list_init(__mutex);

  return mthread_mutex_lock(__mutex);
}

// Wait until lock for MUTEX becomes available and lock it.
int mthread_mutex_lock(mthread_mutex_t *__mutex)
{
  // Added: deleted retval, moved self, moved vp
  if (__mutex == NULL)
    return EINVAL;

  // Added: ensure list is correct
  __mthread_ensure_list_init(__mutex);

  mthread_spinlock_lock(&__mutex->lock);

  if (__mutex->nb_thread == 0)
  {
    __mutex->nb_thread = 1;
    mthread_spinlock_unlock(&__mutex->lock);
  }
  else
  {
    mthread_t self = mthread_self();
    mthread_insert_last(self, __mutex->list);
    self->status = BLOCKED;
    mthread_virtual_processor_t *vp = mthread_get_vp();
    vp->p = &__mutex->lock;
    mthread_spinlock_unlock(&__mutex->lock);
    mthread_yield();
  }

  mthread_log("MUTEX_LOCK", "MUTEX acquired\n");
  return 0;
}

// Unlock MUTEX.
int mthread_mutex_unlock(mthread_mutex_t *__mutex)
{
  // Added: deleted retval, moved first, moved vp
  if (__mutex == NULL)
    return EINVAL;

  // Added: ensure list is correct
  __mthread_ensure_list_init(__mutex);

  mthread_spinlock_lock(&__mutex->lock);
  if (__mutex->list->first != NULL)
  {
    mthread_t first = mthread_remove_first(__mutex->list);
    mthread_virtual_processor_t *vp = mthread_get_vp();
    first->status = RUNNING;
    mthread_insert_last(first, &(vp->ready_list));
  }
  else
  {
    __mutex->nb_thread = 0;
  }

  mthread_spinlock_unlock(&__mutex->lock);

  mthread_log("MUTEX_UNLOCK", "MUTEX released\n");
  return 0;
}
