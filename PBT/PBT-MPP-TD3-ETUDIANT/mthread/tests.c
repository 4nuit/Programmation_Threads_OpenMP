#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
//#include <pthread.h>

#include "mthread.h"

#define NB_THREADS 1

int counter = 0;

mthread_mutex_t normal_mutex;
mthread_mutex_t static_mutex = MTHREAD_MUTEX_INITIALIZER;

mthread_sem_t sem;

void * run(void * arg) {
  printf("Entering run for thread %p\n", mthread_self());
  mthread_mutex_lock(&normal_mutex);
  counter++;
  assert(mthread_mutex_trylock(&normal_mutex) == EBUSY);
  mthread_mutex_unlock(&normal_mutex);

  mthread_mutex_lock(&static_mutex);
  counter++;
  assert(mthread_mutex_trylock(&static_mutex) == EBUSY);
  mthread_mutex_unlock(&static_mutex);

  return NULL;
}

int main(int argc, char ** argv) {
  // Simple tests
  printf("Basic NULL tests ... ");
  fflush(stdout);
  // Mutexes
  assert(mthread_mutex_init(NULL, NULL) == EINVAL);
  assert(mthread_mutex_trylock(NULL) == EINVAL);
  assert(mthread_mutex_lock(NULL) == EINVAL);
  assert(mthread_mutex_unlock(NULL) == EINVAL);
  // Semaphores
  assert(mthread_sem_init(NULL, 1) == EINVAL);
  assert(mthread_sem_init(&sem, 0) == EINVAL);
  assert(mthread_sem_wait(NULL) == EINVAL);
  assert(mthread_sem_post(NULL) == EINVAL);
  assert(mthread_sem_getvalue(NULL, NULL) == EINVAL);
  {
    int sval = 35;
    assert(mthread_sem_getvalue(NULL, &sval) == EINVAL);
    assert(sval == 35);
  }
  assert(mthread_sem_getvalue(&sem, NULL) == EINVAL);
  assert(mthread_sem_trywait(NULL) == EINVAL);
  assert(mthread_sem_destroy(NULL) == EINVAL);
  printf("ok\n");
  fflush(stdout);

  // Initialized mutexes tests
  {
    // Initialization tests
    printf("Initializing normal_mutex ... ");
    fflush(stdout);
    assert(mthread_mutex_init(&normal_mutex, NULL) == 0);
    printf("ok\n");
    fflush(stdout);

    // Normal mutex tests
    {
      printf("Locking normal_mutex ... ");
      fflush(stdout);
      assert(mthread_mutex_lock(&normal_mutex) == 0);
      printf("ok\nTrylock normal mutex ...");
      fflush(stdout);
      assert(mthread_mutex_trylock(&normal_mutex) == EBUSY);
      printf("ok: returned EBUSY\nUnlocking normal mutex ... ");
      fflush(stdout);
      assert(mthread_mutex_unlock(&normal_mutex) == 0);
      printf("ok\n");
      fflush(stdout);

      printf("Trylocking normal_mutex ... ");
      fflush(stdout);
      assert(mthread_mutex_trylock(&normal_mutex) == 0);
      assert(mthread_mutex_trylock(&normal_mutex) == EBUSY);
      printf("Trylock 2 correctly returned EBUSY ... ");
      fflush(stdout);
      assert(mthread_mutex_unlock(&normal_mutex) == 0);
      printf("Unlocked normal_mutex\n");
      fflush(stdout);
    }

    // Static mutex tests
    {
      printf("Locking static_mutex ... ");
      fflush(stdout);
      assert(mthread_mutex_lock(&static_mutex) == 0);
      assert(mthread_mutex_trylock(&static_mutex) == EBUSY);
      printf("Trylock correctly returned EBUSY ... ");
      fflush(stdout);
      assert(mthread_mutex_unlock(&static_mutex) == 0);
      printf("Unlocked static_mutex\n");
      fflush(stdout);

      printf("Trylocking static_mutex ... ");
      fflush(stdout);
      assert(mthread_mutex_trylock(&static_mutex) == 0);
      assert(mthread_mutex_trylock(&static_mutex) == EBUSY);
      printf("Trylock 2 correctly returned EBUSY ... ");
      fflush(stdout);
      assert(mthread_mutex_unlock(&static_mutex) == 0);
      printf("Unlocked static_mutex\n");
      fflush(stdout);
    }

    // Destroy tests
    printf("Destroying static mutex ... ");
    fflush(stdout);
    assert(mthread_mutex_destroy(&static_mutex) == 0);
    printf("ok\n");
    fflush(stdout);
  }

  // Initialized semaphore tests
  {
    const unsigned int value = 2;
    // Initialization test
    printf("Initializing semaphore with value = %d ... ", value);
    fflush(stdout);
    assert(mthread_sem_init(&sem, value) == 0);
    printf("ok\n");
    fflush(stdout);

    int sval = -1;
    for (unsigned int k = 1; k <= value; k++) {
      printf("Wait %d ... ", k);
      fflush(stdout);
      assert(mthread_sem_wait(&sem) == 0);
      assert(mthread_sem_destroy(&sem) == EBUSY);
      assert(mthread_sem_getvalue(&sem, &sval) == 0);
      assert(sval == (int)(value - k));
    }
    printf("ok\n");
    fflush(stdout);

    printf("Trywait ... ");
    fflush(stdout);
    assert(mthread_sem_trywait(&sem) == EBUSY);
    printf("ok\n");
    fflush(stdout);

    for (unsigned int k = 1; k <= value; k++) {
      printf("Post %d ... ", k);
      fflush(stdout);
      assert(mthread_sem_destroy(&sem) == EBUSY);
      assert(mthread_sem_post(&sem) == 0);
      assert(mthread_sem_getvalue(&sem, &sval) == 0);
      assert(sval == (int)k);
    }
    printf("ok\n");
    fflush(stdout);

    // Destroy tests
    printf("Destroying semaphore ... ");
    fflush(stdout);
    assert(mthread_sem_destroy(&sem) == 0);
    printf("ok\n");
    fflush(stdout);
  }

  // Reinit for use in run
  mthread_mutex_init(&normal_mutex, NULL);

  printf("Creating %d mthreads\n", NB_THREADS);
  fflush(stdout);
  mthread_t pids[NB_THREADS] = { 0 };
  for (int k = 0; k < NB_THREADS; k++) {
    mthread_create(&(pids[k]), NULL, run, NULL);
    printf("Createad thread k=%d\n", k);
  }

  printf("Joining %d mthreads\n", NB_THREADS);
  fflush(stdout);
  for (int k = 0; k < NB_THREADS; k++)
    mthread_join(pids[k], NULL);

  printf("Joined.\n");
  fflush(stdout);
  // Destroy after reinit
  mthread_mutex_destroy(&normal_mutex);

  return 0;
}
