#include <omp.h>
/*
 *  Compute forces and accumulate the virial and the potential
 */
extern double epot, vir;

void forces(int npart, double x[], double f[], double side, double rcoff)
{
    int j;
    vir    = 0.0;
    epot   = 0.0;

    const double half_side = 0.5 * side;
    #pragma omp for schedule(runtime) reduction(+:epot) reduction(-:vir) private(j)
    for (int i=0; i<npart*3; i+=3)
    {
        // zero force components on particle i
       
        double fxi = 0.0;
        double fyi = 0.0;
        double fzi = 0.0;

        // loop over all particles with index > i
       
        for (j=i+3; j<npart*3; j+=3)
        {
            // compute distance between particles i and j allowing for wraparound
           
            double xx = x[i]-x[j];
            double yy = x[i+1]-x[j+1];
            double zz = x[i+2]-x[j+2];

            if (xx< (-half_side) ) 
                xx += side;
            else if (xx> (half_side) )
                xx -= side;
            
            if (yy< (-half_side) ) 
                yy += side;
            else if (yy> (half_side) )  
                yy -= side;
            
            if (zz< (-half_side) ) 
                zz += side;
            else if (zz> (half_side) )  
                zz -= side;

            const double rd = xx*xx+yy*yy+zz*zz;

            // if distance is inside cutoff radius compute forces
            // and contributions to pot. energy and virial

            if (rd<=rcoff*rcoff)
            {
                const double rrd      = 1.0/rd;
                const double rrd3     = rrd*rrd*rrd;
                const double rrd4     = rrd3*rrd;
                const double r148     = rrd4*(rrd3 - 0.5);

                epot    += rrd3*(rrd3-1.0);
                vir     -= rd*r148;
                
                const double x148 = xx * r148;
                const double y148 = yy * r148;
                const double z148 = zz * r148;

                fxi += x148;
                fyi += y148;
                fzi += z148;

                #pragma omp atomic
                f[j]    -= xx*r148;
                #pragma omp atomic
                f[j+1]  -= yy*r148;
                #pragma omp atomic
                f[j+2]  -= zz*r148;
            }
        }

        // update forces on particle i
        #pragma omp atomic
        f[i]     += fxi;
        #pragma omp atomic
        f[i+1]   += fyi;
        #pragma omp atomic
        f[i+2]   += fzi;
    }
}
