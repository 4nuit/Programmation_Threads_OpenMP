#ifndef MTHREAD_ADDITIONS
#define MTHREAD_ADDITIONS 1

/**
 * Type of the mutex, used especially in locking and unlocking.
 */
enum mthread_mutex_type {
  /**
   * This type of mutex does not check for usage errors.
   * It will deadlock if reentered, and result in undefined behavior if a locked mutex is unlocked by another thread.
   * Attempts to unlock an already unlocked MTHREAD_MUTEX_NORMAL mutex will result in undefined behavior.
   */
  MTHREAD_MUTEX_NORMAL = 0,
  /**
   * These mutexes do check for usage errors.  If an attempt is made to relock a PTHREAD_MUTEX_ERRORCHECK mutex without first dropping the lock,
   * an error will be returned.  If a thread attempts to unlock a MTHREAD_MUTEX_ERRORCHECK mutex that is locked by another thread, an error will
   * be returned.  If a thread attempts to unlock a MTHREAD_MUTEX_ERRORCHECK thread that is unlocked, an error will be returned.
   */
  MTHREAD_MUTEX_ERRORCHECK,
  /**
   * These mutexes allow recursive locking.
   * An attempt to relock a MTHREAD_MUTEX_RECURSIVE mutex that is already locked by the same thread succeeds.
   * An equivalent number of mthread_mutex_unlock calls are needed before the mutex will wake another thread waiting on this lock.
   * If a thread attempts to unlock a MTHREAD_MUTEX_RECURSIVE mutex that is locked by another thread, an error will be returned.
   * If a thread attempts to unlock a MTHREAD_MUTEX_RECURSIVE thread that is unlocked, an error will be returned.
   */
  MTHREAD_MUTEX_RECURSIVE,
  /**
   * Also this type of mutex will cause undefined behavior if reentered.
   * Unlocking a MTHREAD_MUTEX_DEFAULT mutex locked by another thread will result in undefined behavior.
   * Attempts to unlock an already unlocked MTHREAD_MUTEX_DEFAULT mutex will result in undefined behavior.
   * This is the default mutex type for pthread_mutexaddr_init.
   */
  MTHREAD_MUTEX_DEFAULT,
};
typedef enum mthread_mutex_type mthread_mutex_type_t;

/**
 * Used to correctly schedule the mutex.
 */
enum mthread_mutex_policy {
  MTHREAD_MUTEX_POLICY_FIRST_FIT = 1,
  MTHREAD_MUTEX_POLICY_FAIRSHARE = 3,
};
typedef enum mthread_mutex_policy mthread_mutex_policy_t;

/**
 * Attributes used to parameterize a mutex.
 *
 * - type: see mthread_mutex_type.
 * - protocol: indicates the protocol used when using the mutex, though the (probable) enum is undocumented.
 * - prioceiling: present in the man of pthread_mutexattr but undocumented.
 * - policy: how the mutex is ordonnanced.
 */
struct mthread_mutexattr_s {
  mthread_mutex_type_t type;
  int protocol;
  int prioceiling;
  mthread_mutex_policy_t policy;
};

#endif // MTHREAD_ADDITIONS
