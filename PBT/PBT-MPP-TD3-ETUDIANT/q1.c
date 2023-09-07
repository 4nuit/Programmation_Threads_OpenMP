#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

sem_t mutex;

void *affichage(void*name){
    int i,j;
    for(i=0;i<20;i++){ //nombre de ligne
        sem_wait(&mutex);
        for(j=0;j<5;j++){ //Affiche 5 char
            printf("%s",(char*)name);
        }
        sched_yield();
        for(j=0;j<5;j++){ //Affiche 5 char
            printf("%s",(char*)name);
        }
        printf("\n");
        sem_post(&mutex);
    }
    return NULL;
}
int main(void){
    pthread_t filsA,filsB;
    sem_init(&mutex,0,1);
    if(pthread_create(&filsA,NULL,affichage,"A")){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    if(pthread_create(&filsB,NULL,affichage,"B")){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    if(pthread_join(filsA,NULL)){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    if(pthread_join(filsB,NULL)){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}
