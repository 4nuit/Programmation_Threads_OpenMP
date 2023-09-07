#include "mthread_internal.h"

/* Functions for handling thread-specific data.  */

/* Create a key value identifying a location in the thread-specific
     data area.  Each thread maintains a distinct thread-specific data
     area.  DESTR_FUNCTION, if non-NULL, is called with the value
     associated to that key when the key is destroyed.
     DESTR_FUNCTION is not called if the value associated is NULL when
     the key is destroyed.  */
int mthread_key_create(mthread_key_t *__key, void (*__destr_function)(void *))
{
  not_implemented();
  return 0;
}

/* Destroy KEY.  */
int mthread_key_delete(mthread_key_t __key)
{
  not_implemented();
  return 0;
}

/* Store POINTER in the thread-specific data slot identified by KEY. */
int mthread_setspecific(mthread_key_t __key, const void *__pointer)
{
  not_implemented();
  return 0;
}

/* Return current value of the thread-specific data slot identified by KEY.  */
void *
mthread_getspecific(mthread_key_t __key)
{
  not_implemented();
  return NULL;
}
