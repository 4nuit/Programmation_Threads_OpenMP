#include "mthread_internal.h"

/* Functions for handling conditional variables.  */

/* Initialize condition variable COND using attributes ATTR, or use
     the default values if later is NULL.  */
int mthread_cond_init(mthread_cond_t *__cond,
                      const mthread_condattr_t *__cond_attr)
{
  not_implemented();
  return 0;
}

/* Destroy condition variable COND.  */
int mthread_cond_destroy(mthread_cond_t *__cond)
{
  not_implemented();
  return 0;
}

/* Wake up one thread waiting for condition variable COND.  */
int mthread_cond_signal(mthread_cond_t *__cond)
{
  not_implemented();
  return 0;
}

/* Wake up all threads waiting for condition variables COND.  */
int mthread_cond_broadcast(mthread_cond_t *__cond)
{
  not_implemented();
  return 0;
}

/* Wait for condition variable COND to be signaled or broadcast.
     MUTEX is assumed to be locked before.  */
int mthread_cond_wait(mthread_cond_t *__cond, mthread_mutex_t *__mutex)
{
  not_implemented();
  return 0;
}
