#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NB_THREADS 1000
#define N 1000000
int tab[N];

struct args {
  int start;
  int end;
};

void * run(void * arg) {
  int sum = 0;
  struct args a = *(struct args *)arg;
  for (int i = a.start; i < a.end; i++)
    sum += tab[i];
  return (void *)sum;
}

int main(int argc, char ** argv) {

  pthread_t pids[NB_THREADS];
  struct args args[NB_THREADS];
  int section_size = N / NB_THREADS;

  for (int i = 0; i < N; i++)
    tab[i] = 1;

  for (int i = 0; i < NB_THREADS; i++)
    args[i] = (struct args){ .start = i * section_size, .end = (i + 1) * section_size };

  clock_t start_time = clock();

  for (int k = 0; k < NB_THREADS; k++)
    pthread_create(&pids[k], NULL, run, &(args[k]));

  int res[NB_THREADS] = { 0 };
  for (int k = 0; k < NB_THREADS; k++)
    pthread_join(pids[k], (void **)&res[k]);

  int s = 0;
  for (int i = 0; i < NB_THREADS; i++)
    s += res[i];

  clock_t end_time = clock();
  double cpu_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

  printf("s = %d\n", s);
  printf("CPU time: %f seconds\n", cpu_time);

  return 0;
}
