#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>

//Question 1

void *hello(void *arg){
    printf("Hello World! number=%d pid=%d pthread_self=%p\n",arg,getpid(),(void *) pthread_self());
    return NULL;
}

int main(int argc, char **argv){
    int i,nb;
    nb = atoi(argv[1]);
    pthread_t *threads;
    threads = malloc(nb*sizeof(pthread_t));
    for(i=0; i<nb; i++){
        pthread_create(&threads[i],NULL,hello,NULL);
    }
    printf("Début de l'attente\n");
    for(i=0; i<nb; i++){
        pthread_join(&threads[i],NULL);
    }
    printf("Fin de l'attente\n");
    return 0;
}
