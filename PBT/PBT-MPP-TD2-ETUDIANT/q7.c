#include <pth.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#define NB_THREADS 8

void * run(void * arg) {
	sleep(2);
	return NULL;
}

int main(int argc, char ** argv) {
	clock_t start_cpu, end_cpu;
	time_t start, end;

	if (!pth_init()) return 1;

	start = time(NULL);
	pth_t pids[NB_THREADS];
	
	for (int k = 0; k < NB_THREADS; k++)
		pids[k] = pth_spawn(PTH_ATTR_DEFAULT, run, NULL);

	start_cpu = clock();
	for (int k = 0; k < NB_THREADS; k++)
		pth_join(pids[k], NULL);

	end_cpu = clock();
	pth_kill();

	end = time(NULL);
	double cpu_time_used = ((double) (end_cpu - start_cpu)) / CLOCKS_PER_SEC;
	double time_total = difftime(end, start);
	printf("CPU time used: %f seconds\n", cpu_time_used);
	printf("Total time used: %f seconds\n", time_total);
	return 0;
}
