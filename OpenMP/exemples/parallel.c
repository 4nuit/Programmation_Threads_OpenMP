#include <stdio.h>
#include <omp.h>

int main(int argc, char **argv){
	int N;
	N = 10;
	#pragma omp parallel
	{
		int i;
	#pragma omp for schedule(static)
		for(i=0;i<N;i++){
			printf("Thread %d running iteration %d\n",omp_get_thread_num(),i);
		}
	}
	return 0;
}
