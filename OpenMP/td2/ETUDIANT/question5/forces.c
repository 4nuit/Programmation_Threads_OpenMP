#include <omp.h>
/*
 *  Compute forces and accumulate the virial and the potential
 */
extern double epot, vir;

void forces(const int npart, const double x[], double f[], const double side, const double rcoff, double **tmpf, const int nb_threads) {
  #pragma omp single
  {
    vir = 0.0;
    epot = 0.0;
  }
  const double halfside = 0.5 * side;
  const double minushalfside = -0.5 * side;
  const int thread_rank = omp_get_thread_num();

  #pragma omp for schedule(runtime) reduction(+:epot) reduction(-:vir)
  for (int ip = 0; ip < npart; ip += 1) {
    const int i = ip * 3;
    // zero force components on particle i
    double fxi = 0.0;
    double fyi = 0.0;
    double fzi = 0.0;

    // loop over all particles with index > i
    for (int j = i + 3; j < npart * 3; j += 3) {
      // compute distance between particles i and j allowing for wraparound

      double xx = x[i] - x[j];
      double yy = x[i+1] - x[j+1];
      double zz = x[i+2] - x[j+2];

      if (xx < (minushalfside))
        xx += side;
      else if (xx > (halfside))
        xx -= side;

      if (yy < (minushalfside))
        yy += side;
      else if (yy > (halfside))
        yy -= side;

      if (zz < (minushalfside))
        zz += side;
      else if (zz > (halfside))
        zz -= side;

      const double rd = xx * xx + yy * yy + zz * zz;

      // if distance is inside cutoff radius compute forces
      // and contributions to pot. energy and virial
      if (rd <= rcoff * rcoff) {
        const double rrd = 1.0 / rd;
        const double rrd3 = rrd * rrd * rrd;
        const double rrd4 = rrd3 * rrd;
        const double r148 = rrd4 * (rrd3 - 0.5);

        epot += rrd3 * (rrd3 - 1.0);
        vir -= rd * r148;

        const double xx148 = xx * r148;
        const double yy148 = yy * r148;
        const double zz148 = zz * r148;

        fxi += xx148;
        fyi += yy148;
        fzi += zz148;

        tmpf[thread_rank][j] -= xx148;
        tmpf[thread_rank][j+1] -= yy148;
        tmpf[thread_rank][j+2] -= zz148;
      }
    }

    // update forces on particle i
    tmpf[thread_rank][i] += fxi;
    tmpf[thread_rank][i+1] += fyi;
    tmpf[thread_rank][i+2] += fzi;
  }

  #pragma omp for schedule(runtime)
  for(int i = 0; i < 3 * npart; i++) {
    f[i] = 0.0;
    for(int r = 0; r < nb_threads; r++) {
      // Il est ennuyant d'utiliser [r][i], car on boucle sur [i][r],
      // mais la dépendance à f[i] empêche un changement qui fixerait ça
      f[i] += tmpf[r][i];
      tmpf[r][i] = 0.0; // Pour la prochaine itération
    }
  }

}
