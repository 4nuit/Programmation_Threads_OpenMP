#include <omp.h>
/*
 *  Compute forces and accumulate the virial and the potential
 */
extern double epot, vir;

void forces(const int npart, const double x[], double f[], const double side, const double rcoff) {
  #pragma omp single
  {
    vir = 0.0;
    epot = 0.0;
  }
  const double halfside = 0.5 * side;
  const double minushalfside = -0.5 * side;

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

        // Des opérations atomiques (ici et en dessous) sont nécessaires pour éviter
        // toute forme d'erreur lors des calculs (data race)
        #pragma omp atomic
        f[j] -= xx148;
        #pragma omp atomic
        f[j+1] -= yy148;
        #pragma omp atomic
        f[j+2] -= zz148;
      }
    }

    // update forces on particle i
    #pragma omp atomic
    f[i] += fxi;
    #pragma omp atomic
    f[i+1] += fyi;
    #pragma omp atomic
    f[i+2] += fzi;
  }
}
