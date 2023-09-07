#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>
#include <time.h>

int
main(int argc, char** argv)
{
    uint64_t n_test = 1E7;
    uint64_t i;
    uint64_t count = 0;
    double x = 0., y = 0.;
    double pi = 0.;

    srand(2023);
    clock_t start = clock();
    for(i = 0; i < n_test; ++i)
    {
        x = rand() / (double)RAND_MAX;
        y = rand() / (double)RAND_MAX;
        count += (((x * x) + (y * y)) <= 1);
    }

    clock_t end = clock();
    fprintf(stdout, "%ld of %ld throws are in the circle !\n", count, n_test);
    pi = 4 * count / (double)n_test;
    fprintf(stdout, "Pi ~= %lf\n", pi);
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
    fprintf(stdout, "Execution time: %lf seconds\n", time_spent);
    return 0;
}
