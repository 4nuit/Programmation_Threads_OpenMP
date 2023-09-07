#include <omp.h>
#include <stdio.h>

float a;
int main(){
	a = 92000;
	#pragma omp parallel
	{
		sub();
	}
	return 0;
}

float a;

void sub(void){
	float b;
	
	b = a +290.;
	printf("b vaut : %f\n",b);
}
