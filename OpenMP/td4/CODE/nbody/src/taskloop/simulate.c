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
            int iter = 0;
            while (iter < N_ITERS)
            {
                update_forces();
                update_acceleration();
                update_velocity(DT);
                update_position(DT);
                ++iter;
                if (iter % (int)(0.1 * N_ITERS) == 0)
                {
                    printf("iteration %d/%d - position=%lf\n", iter, N_ITERS, bodies_checksum());
                }
            }
        }
    }
}
