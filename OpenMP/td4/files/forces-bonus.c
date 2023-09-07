/**
 *  à propos de cette implémentation
 *
 *  - le type de dépendances « inoutset » n'est pas encore bien supportée
 *  par les compilateurs et les runtimes. Le comportement de ce type
 *  de dépendances peut être reproduit en utilisant
 *  des dépendances de données factices : plusieurs 'out' sur
 *  les tâches concurrentes et plusieurs 'inout' sur la tâche de réduction
 *
 *  Exemple (inoutset) :
 *
 *  for (int i = 0 ; i < N ; ++i)
 *  {
 *      task depend(inoutset: x)        // tâches concurrentes
 *  }
 *  task depend(inout: x)               // reduction
 *
 *  Équivalent en factice
 *
 *  for (int i = 0 ; i < N ; ++i)
 *  {
 *      task depend(out: factices[i])               // tâches concurrentes
 *  }
 *  task depend(iterator(i=0:N), in: factices[i])   // réduction
 */
# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# include "body.h"

# include <mpc_omp_task_label.h>

static vec3_t * per_thread_forces = NULL;
static char * virtual_deps = NULL;
static int nthreads = 0;

void
update_forces(void)
{
    for (int nt = 0 ; nt < TASKS_PER_LOOP ; ++nt)
    {
        MPC_OMP_TASK_SET_LABEL("forces_compute");
        # pragma omp task default(none)                                 \
            firstprivate(nt)                                            \
            shared(bodies, nthreads, per_thread_forces)                 \
            depend( iterator(nt2=nt:TASKS_PER_LOOP),                    \
                        out: virtual_deps[nt * TASKS_PER_LOOP + nt2])
        {
            int t = omp_get_thread_num();
            int k = nt * BODIES_PER_TASK;
            int end = MIN(N_BODIES, k + BODIES_PER_TASK);
            for (int i = k ; i < end ; ++i)
            {
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
        }
    }

    for (int nt = 0 ; nt < TASKS_PER_LOOP ; ++nt)
    {
        MPC_OMP_TASK_SET_LABEL("forces_reduction");
        # pragma omp task default(none)                                 \
            firstprivate(nt)                                            \
            shared(bodies, nthreads, per_thread_forces)                 \
            depend( iterator(nt2=0:nt+1),                               \
                        inout: virtual_deps[nt + nt2 * TASKS_PER_LOOP]) \
            depend(out: bodies[nt * BODIES_PER_TASK].forces)
        {
            int start = nt * BODIES_PER_TASK;
            vec3_t * forces = per_thread_forces + start * nthreads;
            int end = MIN(N_BODIES, start + BODIES_PER_TASK);
            for (int i = start ; i < end ; ++i)
            {
                body_t * body = bodies + i;
                body->forces.x = 0.0;
                body->forces.y = 0.0;
                body->forces.z = 0.0;

                for (int t = 0 ; t < nthreads ; ++t)
                {
                    body->forces.x += forces[t].x;
                    body->forces.y += forces[t].y;
                    body->forces.z += forces[t].z;

                    forces[t].x = 0.0;
                    forces[t].y = 0.0;
                    forces[t].z = 0.0;
                }

                forces += nthreads;
            }
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

            virtual_deps = (char *) malloc(TASKS_PER_LOOP * TASKS_PER_LOOP);
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
            free(virtual_deps);
        }
    }
}
