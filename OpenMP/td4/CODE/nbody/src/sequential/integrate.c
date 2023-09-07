# include "body.h"

void
update_acceleration(void)
{
    for (int i = 0 ; i < N_BODIES ; i++)
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
    for (int i = 0 ; i < N_BODIES ; i++)
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
    for (int i = 0 ; i < N_BODIES ; i++)
    {
        body_t * body = bodies + i;
        body->position.x = body->position.x + dt * body->velocity.x;
        body->position.y = body->position.y + dt * body->velocity.y;
        body->position.z = body->position.z + dt * body->velocity.z;
    }
}
