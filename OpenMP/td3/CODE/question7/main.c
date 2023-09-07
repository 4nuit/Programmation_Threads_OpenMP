#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include "omp.h"

#include "CSRMatrix.h"

int main(int argc, char** argv)
{
  double *x = NULL, *y = NULL;
  int nx = 100, ny = 100, nrows = 0;
  int nb_test = 1, i = 0, j = 0;
  int check = 1;
  double t0 = 0., t1 = 0., duration = 0.;
  double norme= 0. ;
  CSRMatrix_t* A = NULL;

  if(argc > 1) nx = atoi(argv[1]);
  if(argc > 2) ny = atoi(argv[2]);
  if(argc > 3) nb_test = atoi(argv[3]);
  if(argc > 4) check = atoi(argv[4]);
  nrows = nx * ny;

  fprintf(stdout, "NX: %d\tNY: %d\nNTest: %d\n", nx, ny, nb_test);
  A = (CSRMatrix_t*) malloc( sizeof(CSRMatrix_t) );
  x = (double*) malloc( nrows * sizeof(double) );
  y = (double*) malloc( nrows * sizeof(double) );
  for(i = 0; i < nrows; ++i)
  {
    x[i] = 1. * i ;
    y[i] = 0. ;
  }

  buildLaplacian(A,nx,ny) ;

  #ifdef DEBUG
  print_CSR(A);
  #endif

  for(i = 0; i < nrows; ++i)
    x[i] = 1. * i ;

  for(i = 0; i < nb_test; ++i)
  {
    t0 = get_elapsedtime();

    mult_CSR(A,x,y) ;

    t1 = get_elapsedtime();
    duration += (t1 - t0);

    norme=0. ;
    for(j=0;j<nrows;++j)
      norme += y[j]*y[j] ;
    norme = sqrt(norme) ;
    for(j=0;j<nrows;++j)
      x[j] = y[j]/norme ;
  }

  if(check)
  {
    double norme=0. ;
    for(i=0;i<nrows;++i)
       norme += y[i]*y[i] ;
    fprintf(stdout, "NORME Y= %.2f\n",sqrt(norme)) ;
  }

  fprintf(stdout, " Time : %f\n", duration);
  uint64_t flop_csr = (unsigned long long)(A->m_nnz) * 2;
  fprintf(stdout, " MFlops : %.2f\n", flop_csr / (duration/nb_test)*1E-6);
  fprintf(stdout, "AvgTime : %f\n", duration/nb_test);

  free(x);
  free(y);
  destruct_CSR(A);
  free(A);

  return 0;
}
