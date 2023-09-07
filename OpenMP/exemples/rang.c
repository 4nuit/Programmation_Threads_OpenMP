#include <omp.h>
#include <stdio.h>

int main(){
	int rang;
	#pragma omp parallel private(rang) \
		num_threads(3)
	{
		rang = omp_get_thread_num();
		printf("Mon rang dans la region 1: %d\n",rang);
	
		#pragma omp parallel private(rang) \
			num_threads(2)
		{
			rang = omp_get_thread_num();
			printf("Mon rang dans la region 2 : %d\n",rang);
		}
	}
	return 0;
}
