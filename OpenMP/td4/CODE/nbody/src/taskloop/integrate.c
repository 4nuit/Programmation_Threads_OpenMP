# include <stdio.h>
# include <stdlib.h>

# include "body.h"

# include <mpc_omp_task_label.h>

void
update_acceleration(void)
{
    MPC_OMP_TASK_SET_LABEL("acceleration");
    # pragma omp taskloop grainsize(BODIES_PER_TASK) default(none) shared(bodies)
    for (int i = 0 ; i < N_BODIES ; ++i)
    {
        body_t * body = bodies + i;
        body->acceleration.x = body->forces.x / body->mass;
        body->acceleration.y = body->forces.y / body->mass;
        body->acceleration.z = body->forces.z / body->mass;
    }
}

void
update_velocity(double dt)
{
    MPC_OMP_TASK_SET_LABEL("velocity");
    # pragma omp taskloop grainsize(BODIES_PER_TASK) default(none) shared(bodies, dt)
    for (int i = 0 ; i < N_BODIES ; ++i)
    {
        body_t * body = bodies + i;
        body->velocity.x = body->velocity.x + dt * body->acceleration.x;
        body->velocity.y = body->velocity.y + dt * body->acceleration.y;
        body->velocity.z = body->velocity.z + dt * body->acceleration.z;
    }
}

void
update_position(double dt)
{
    MPC_OMP_TASK_SET_LABEL("position");
    # pragma omp taskloop grainsize(BODIES_PER_TASK) default(none) shared(bodies, dt)
    for (int i = 0 ; i < N_BODIES ; ++i)
    {
        body_t * body = bodies + i;
        body->position.x = body->position.x + dt * body->velocity.x;
        body->position.y = body->position.y + dt * body->velocity.y;
        body->position.z = body->position.z + dt * body->velocity.z;
    }
}
