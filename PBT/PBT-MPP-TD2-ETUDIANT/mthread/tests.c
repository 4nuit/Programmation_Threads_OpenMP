#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include "mthread.h"

#define NB_THREADS 64

int counter = 0;

mthread_mutex_t normal_mutex;
mthread_mutex_t static_mutex = MTHREAD_MUTEX_INITIALIZER;

void * run(void * arg) {
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
  assert(mthread_mutex_init(NULL, NULL) == EINVAL);
  assert(mthread_mutex_trylock(NULL) == EINVAL);
  assert(mthread_mutex_lock(NULL) == EINVAL);
  assert(mthread_mutex_unlock(NULL) == EINVAL);
  printf("ok\n");
  fflush(stdout);

  // Initialization tests
  printf("Initializing normal_mutex ... ");
  fflush(stdout);
  mthread_mutex_init(&normal_mutex, NULL);
  printf("ok\n");
  fflush(stdout);

  // Normal mutex tests
  {
    printf("Locking normal_mutex ... ");
    fflush(stdout);
    assert(mthread_mutex_lock(&normal_mutex) == 0);
    assert(mthread_mutex_trylock(&normal_mutex) == EBUSY);
    printf("Trylock correctly returned EBUSY ... ");
    fflush(stdout);
    assert(mthread_mutex_unlock(&normal_mutex) == 0);
    printf("Unlocked normal_mutex\n");
    fflush(stdout);

    printf("Locking normal_mutex ... ");
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

    printf("Locking static_mutex ... ");
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
  printf("Destroying normal_mutex ... ");
  fflush(stdout);
  mthread_mutex_destroy(&normal_mutex);
  printf("ok\n");
  fflush(stdout);

  // Reinit for use in run
  mthread_mutex_init(&normal_mutex, NULL);

  printf("Creating %d mthreads\n", NB_THREADS);
  fflush(stdout);
  mthread_t pids[NB_THREADS] = { 0 };
  for (int k = 0; k < NB_THREADS; k++)
    mthread_create(&(pids[k]), NULL, run, NULL);

  printf("Joining %d mthreads\n", NB_THREADS);
  fflush(stdout);
  for (int k = 0; k < NB_THREADS; k++)
    mthread_join(pids[k], NULL);

  printf("Joined %d mthreads\n", NB_THREADS);
  fflush(stdout);
  // Destroy after reinit
  mthread_mutex_destroy(&normal_mutex);

  return 0;
}
