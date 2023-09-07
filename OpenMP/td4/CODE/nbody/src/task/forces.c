# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# include "body.h"

# include <mpc_omp_task_label.h>

static vec3_t * per_thread_forces = NULL;
static int nthreads = 0;

void
update_forces(void)
{
    MPC_OMP_TASK_SET_LABEL("forces_compute");
    # pragma omp taskloop grainsize(BODIES_PER_TASK) default(none) \
        shared(bodies, per_thread_forces, nthreads)
    for (int i = 0 ; i < N_BODIES ; ++i)
    {
        int t = omp_get_thread_num();
        body_t * b1 = bodies + i;
        vec3_t * forces;
        for (int j = i + 1 ; j < N_BODIES ; j++)
        {
            body_t * b2 = bodies + j;

            double dx = b2->position.x - b1->position.x;
            double dy = b2->position.y - b1->position.y;
            double dz = b2->position.z - b1->position.z;
            double r = sqrt(dx * dx + dy * dy + dz * dz);

            double f = G * b1->mass * b2->mass / (r * r);
            double fx = dx / r * f;
            double fy = dy / r * f;
            double fz = dz / r * f;

            forces = per_thread_forces + i * nthreads;
            forces[t].x +=  fx;
            forces[t].y +=  fy;
            forces[t].z +=  fz;

            forces = per_thread_forces + j * nthreads;
            forces[t].x += -fx;
            forces[t].y += -fy;
            forces[t].z += -fz;
        }
    }

    MPC_OMP_TASK_SET_LABEL("forces_reduction");
    # pragma omp taskloop grainsize(BODIES_PER_TASK) default(none) \
        shared(bodies, per_thread_forces, nthreads)
    for (int i = 0 ; i < N_BODIES ; ++i)
    {
        body_t * body = bodies + i;
        body->forces.x = 0.0;
        body->forces.y = 0.0;
        body->forces.z = 0.0;

        int nthreads = omp_get_num_threads();
        vec3_t * forces = per_thread_forces + i * nthreads;
        for (int t = 0 ; t < nthreads ; ++t)
        {
            body->forces.x += forces[t].x;
            body->forces.y += forces[t].y;
            body->forces.z += forces[t].z;

            forces[t].x = 0.0;
            forces[t].y = 0.0;
            forces[t].z = 0.0;
        }
    }
}

void
init(void)
{
    # pragma omp parallel shared(nthreads)
    {
        # pragma omp master
        {
            nthreads = omp_get_num_threads();

            per_thread_forces = (vec3_t *) malloc(sizeof(vec3_t) * nthreads * N_BODIES);
            memset(per_thread_forces, 0, sizeof(vec3_t) * nthreads * N_BODIES);
        }
    }
}

void
deinit(void)
{
    # pragma omp parallel
    {
        # pragma omp master
        {
            free(per_thread_forces);
        }
    }
}
