# include <stdio.h>

# include "body.h"
# include "parameters.h"
# include "simulate.h"

void
simulate(void)
{
    # pragma omp parallel
    {
        # pragma omp single
        {
            double t0 = omp_get_wtime();
            int iter = 0;
            while (iter < N_ITERS)
            {
                update_forces();
                update_acceleration();
                update_velocity(DT);
                update_position(DT);
                ++iter;
                # pragma omp task default(none) firstprivate(iter) depend(in: bodies[0])
                {
                    if (iter % (int)(0.1 * N_ITERS) == 0)
                    {
                        printf("iteration %d/%d - position=%lf\n", iter, N_ITERS, bodies_checksum());
                    }
                }
            }
            double tf = omp_get_wtime();
            printf("Graph generation took %lf s.\n", tf - t0);
        }
    }
}
