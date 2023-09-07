# include <assert.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# include <mpc_omp_task_label.h>

# include "body.h"

void
update_acceleration(void)
{
    for (int k = 0 ; k < N_BODIES ; k += BODIES_PER_TASK)
    {
        MPC_OMP_TASK_SET_LABEL("acceleration");
        /* TODO AJOUTER DEPENDANCES DE DONNEES ICI */
        # pragma omp task default(none) firstprivate(k) shared(bodies, BODIES_PER_TASK)
        {
            int end = MIN(N_BODIES, k + BODIES_PER_TASK);
            for (int i = k ; i < end ; ++i)
            {
                body_t * body = bodies + i;
                body->acceleration.x = body->forces.x / body->mass;
                body->acceleration.y = body->forces.y / body->mass;
                body->acceleration.z = body->forces.z / body->mass;
            }
        }
    }
}

void
update_velocity(double dt)
{
    for (int k = 0 ; k < N_BODIES ; k += BODIES_PER_TASK)
    {
        MPC_OMP_TASK_SET_LABEL("velocity");
        /* TODO AJOUTER DEPENDANCES DE DONNEES ICI */
        # pragma omp task default(none) firstprivate(k, dt) shared(bodies, BODIES_PER_TASK)
        {
            int end = MIN(N_BODIES, k + BODIES_PER_TASK);
            for (int i = k ; i < end ; ++i)
            {
                body_t * body = bodies + i;
                body->velocity.x = body->velocity.x + dt * body->acceleration.x;
                body->velocity.y = body->velocity.y + dt * body->acceleration.y;
                body->velocity.z = body->velocity.z + dt * body->acceleration.z;
            }
        }
    }
}

void
update_position(double dt)
{
    for (int k = 0 ; k < N_BODIES ; k += BODIES_PER_TASK)
    {
        MPC_OMP_TASK_SET_LABEL("position");
        /* TODO AJOUTER DEPENDANCES DE DONNEES ICI */
        # pragma omp task default(none) firstprivate(k, dt) shared(bodies, BODIES_PER_TASK)
        {
            int end = MIN(N_BODIES, k + BODIES_PER_TASK);
            for (int i = k ; i < end ; ++i)
            {
                body_t * body = bodies + i;
                body->position.x = body->position.x + dt * body->velocity.x;
                body->position.y = body->position.y + dt * body->velocity.y;
                body->position.z = body->position.z + dt * body->velocity.z;
            }
        }
    }
}
