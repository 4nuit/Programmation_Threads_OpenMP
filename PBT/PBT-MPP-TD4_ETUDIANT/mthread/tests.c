#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "mthread.h"

#define NB_THREADS 5
// Defines that will help specialize the tests if needed
#define NB_THREADS_MUTEX_TEST \
  NB_THREADS
#define NB_THREADS_SEM_TEST \
  NB_THREADS
#define NB_THREADS_COND_SIGNAL_TEST \
  NB_THREADS
#define NB_THREADS_COND_BROADCAST_TEST \
  NB_THREADS

void inc_and_print(const long thread_num)
{
  static int counter = 0;
  counter++;
  fprintf(stderr, "[%ld] Counter is: %d\n", thread_num, counter);
}

mthread_mutex_t mutex = MTHREAD_MUTEX_INITIALIZER;
void *test_mutex(void *arg)
{
  const long thread_num = (long)arg;
  fprintf(stderr, "[%ld] Entering test_mutex() :: %p\n", thread_num, mthread_self());

  // There should never be a data race on counter here
  mthread_mutex_lock(&mutex);
  fprintf(stderr, "[%ld] Inside mutex lock\n", thread_num);
  assert(mthread_mutex_trylock(&mutex) == EBUSY);
  assert(mthread_mutex_destroy(&mutex) == EBUSY);
  inc_and_print(thread_num);
  mthread_mutex_unlock(&mutex);

  fprintf(stderr, "[%ld] Outside mutex lock\n", thread_num);
  return NULL;
}

mthread_sem_t sem = MTHREAD_SEM_INITIALIZER(2);
void *test_sem(void *arg)
{
  const long thread_num = (long)arg;
  fprintf(stderr, "[%ld] Entering test_sem() :: %p\n", thread_num, mthread_self());

  // Data races on counter here are possibles since several thread can enter the semaphore at the same time
  mthread_sem_wait(&sem);
  fprintf(stderr, "[%ld] Inside sem wait\n", thread_num);
  inc_and_print(thread_num);
  mthread_sem_post(&sem);

  fprintf(stderr, "[%ld] Outside sem wait\n", thread_num);
  return NULL;
}

mthread_cond_t cond_signal = MTHREAD_COND_INITIALIZER;
mthread_mutex_t mutex_signal = MTHREAD_MUTEX_INITIALIZER;
void *test_cond_signal(void *arg)
{
  const long thread_num = (long)arg;
  fprintf(stderr, "[%ld] Entering test_cond_signal() :: %p\n", thread_num, mthread_self());

  if (thread_num == 0)
  {
    sleep(10);
    fprintf(stderr, "[%ld] Starting signaling\n", thread_num);
    for (int k = 1; k < NB_THREADS; k++)
    {
      mthread_cond_signal(&cond_signal);
      sleep(1);
    }
    fprintf(stderr, "[%ld] Finished signaling\n", thread_num);
  }
  else
  {
    mthread_mutex_lock(&mutex_signal);
    fprintf(stderr, "[%ld] Waiting for cond signal\n", thread_num);
    mthread_cond_wait(&cond_signal, &mutex_signal);
    fprintf(stderr, "[%ld] Got signaled\n", thread_num);
    mthread_mutex_unlock(&mutex_signal);
  }

  return NULL;
}

mthread_mutex_t mutex_broadcast = MTHREAD_MUTEX_INITIALIZER;
mthread_cond_t cond_broadcast = MTHREAD_COND_INITIALIZER;
void *test_cond_broadcast(void *arg)
{
  const long thread_num = (long)arg;
  fprintf(stderr, "[%ld] Entering test_cond_broadcast() :: %p\n", thread_num, mthread_self());

  if (thread_num == 0)
  {
    sleep(10);
    fprintf(stderr, "[%ld] Starting broadcasting\n", thread_num);
    mthread_cond_broadcast(&cond_broadcast);
    fprintf(stderr, "[%ld] Finished broadcasting\n", thread_num);
  }
  else
  {
    mthread_mutex_lock(&mutex_broadcast);
    fprintf(stderr, "[%ld] Waiting for cond broadcast\n", thread_num);
    mthread_cond_wait(&cond_broadcast, &mutex_broadcast);
    fprintf(stderr, "[%ld] Got broadcasted\n", thread_num);
    mthread_mutex_unlock(&mutex_broadcast);
  }

  return NULL;
}

void test(const char *name, const int nb_threads, void *(routine)(void *))
{
  fprintf(stderr, "== Starting tests - %s ==\n", name);

  mthread_t pids[nb_threads];
  for (long k = 0; k < nb_threads; k++)
  {
    mthread_create(&(pids[k]), NULL, routine, (void *)k);
  }

  for (int k = 0; k < nb_threads; k++)
  {
    mthread_join(pids[k], NULL);
  }

  fprintf(stderr, "== Finished tests - %s ==\n\n", name);
}

int main(int argc, char **argv)
{
  fprintf(stderr, "==== Starting the tests ====\n\n");

  test("Mutex", NB_THREADS_MUTEX_TEST, test_mutex);
  sleep(5);
  test("Semaphore", NB_THREADS_SEM_TEST, test_sem);
  sleep(5);
  test("Cond Signal", NB_THREADS_COND_SIGNAL_TEST, test_cond_signal);
  sleep(5);
  test("Cond Broadcast", NB_THREADS_COND_BROADCAST_TEST, test_cond_broadcast);

  fprintf(stderr, "==== The tests were successful ====\n");

  return 0;
}
