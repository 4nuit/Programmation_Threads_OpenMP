#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>

#include <omp.h>

#define N_TASKS_PER_THREAD 1
#define N_TRIALS_PER_TASK 1E7

int main(int argc, char ** argv) {
  uint64_t count = 0;
  uint64_t nb_tests;

  srand(2023);

  const double start = omp_get_wtime();
  #pragma omp parallel shared(count,nb_tests)
  {
    #pragma omp single
    {
      nb_tests = N_TASKS_PER_THREAD * N_TRIALS_PER_TASK * omp_get_num_threads();
    }

    uint64_t local_count = 0;

    uint64_t local_task_counts[N_TASKS_PER_THREAD] = { 0 };
    for (int i = 0; i < N_TASKS_PER_THREAD; i++) {
      #pragma omp task firstprivate(i) shared(local_task_counts)
      {
        for (int k = 0; k < N_TRIALS_PER_TASK; k++ ) {
          const double x = rand() / (double)RAND_MAX;
          const double y = rand() / (double)RAND_MAX;

          local_task_counts[i] += (((x * x) + (y * y)) <= 1);
        }
      }
    }
    #pragma omp taskwait

    for (int i = 0; i < N_TASKS_PER_THREAD; i++) {
      local_count += local_task_counts[i];
    }

    #pragma omp atomic
    count += local_count;
  }
  const double end = omp_get_wtime();

  fprintf(stdout, "%llu of %llu throws are in the circle !\n", count, nb_tests);
  const double pi = ((double)count * 4) / (double)nb_tests;
  fprintf(stdout, "Pi ~= %lf\n", pi);
  fprintf(stdout, "Execution time: %fs\n", end - start);

  return 0;
}
