#include <stdio.h>
#include <omp.h>

double sum;
/*
 * Scale forces, update velocities and compute K.E.
 */
double mkekin(const int npart, double f[], double vh[], const double hsq2, const double hsq) {

  #pragma omp single
  {
    sum = 0.0;
  }
  #pragma omp for reduction(+:sum) schedule(runtime)
  for (int i = 0; i < 3 * npart; i++) {
    f[i] *= hsq2;
    vh[i] += f[i];
    sum += vh[i] * vh[i];
  }
  const double ekin = sum / hsq;
  return ekin;
}
