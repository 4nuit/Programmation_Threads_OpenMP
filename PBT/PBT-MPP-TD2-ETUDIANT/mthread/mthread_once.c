#include "mthread_internal.h"
/* Functions for handling initialization.  */

/* Guarantee that the initialization function INIT_ROUTINE will be called
     only once, even if mthread_once is executed several times with the
     same ONCE_CONTROL argument. ONCE_CONTROL must point to a static or
     extern variable initialized to MTHREAD_ONCE_INIT.

     The initialization functions might throw exception which is why
     this function is not marked with .  */
int mthread_once(mthread_once_t *__once_control, void (*__init_routine)(void))
{
  not_implemented();
  return 0;
}
