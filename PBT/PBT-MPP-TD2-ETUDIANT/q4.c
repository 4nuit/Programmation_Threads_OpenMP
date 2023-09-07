#include <pth.h>
#include <stdio.h>

#define NB_THREADS 16
#define N NB_THREADS * 100000000
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

  if (!pth_init()) return 1;

  for (int i = 0; i < N; i++)
    tab[i] = 1;


  pth_t pids[NB_THREADS];
  struct args args[NB_THREADS];

  for (int i = 0; i < NB_THREADS; i++)
    args[i] = (struct args){ .start = i * N, .end = (i + 1) * N };

  for (int k = 0; k < NB_THREADS; k++)
    pids[k] = pth_spawn(PTH_ATTR_DEFAULT, run, &(args[k]));

  int res[NB_THREADS] = { 0 };
  for (int k = 0; k < NB_THREADS; k++)
    pth_join(pids[k], (void **)&res[k]);

  pth_kill();

  int s = 0;
  for (int i = 0; i < NB_THREADS; i++)
    s += res[i];

  printf("s = %d\n", s);

  return 0;
}
