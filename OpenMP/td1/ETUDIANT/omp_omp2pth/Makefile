all: omp pthread

omp:
	gcc -Wall -Werror -Wextra -fopenmp prog_omp.c -o prog_omp

pthread:
	gcc -Wall -Werror -Wextra -fopenmp prog_pthread.c -o prog_pthread

clean:
	rm prog_omp prog_pthread
