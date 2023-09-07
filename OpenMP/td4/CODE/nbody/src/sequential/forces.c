# include "body.h"

void
update_forces(void)
{
    for (int i = 0 ; i < N_BODIES ; i++)
    {
        body_t * body = bodies + i;
        body->forces.x = 0.0;
        body->forces.y = 0.0;
        body->forces.z = 0.0;
    }

    for (int i = 0 ; i < N_BODIES ; i++)
    {
        body_t * b1 = bodies + i;
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

            b1->forces.x +=  fx;
            b1->forces.y +=  fy;
            b1->forces.z +=  fz;

            b2->forces.x += -fx;
            b2->forces.y += -fy;
            b2->forces.z += -fz;
        }
    }
}

void
init(void)
{}

void
deinit(void)
{}
