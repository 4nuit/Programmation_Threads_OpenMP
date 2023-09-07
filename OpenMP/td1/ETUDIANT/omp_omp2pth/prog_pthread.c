#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s [N]\n", argv[0]);
        return 1;
    }

    int N = atoi(argv[1]);
    int sum = 0;

    /* TODO : remplir entre ici */
    (void) N;
    /* TODO : et là */

    printf("sum = %d\n", sum);

    return 0;
}

