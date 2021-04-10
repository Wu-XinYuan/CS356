#include <stdlib.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

sem_t rack_available, burger_ready, cashier_ready, customer_here, customer_none, leave_mutex;
int customer_left;

/* thread for cooks
 * always cooking unless there is no rack*/
void *cook(void *i){
    int id = *((int*)i);
    while (1){
        sem_wait(&rack_available);
        printf("cook[%d] make a burger\n", id);
        sem_post(&burger_ready);
    }
}

/* thread for cashiers
 * wake up when customer comes
 * then wait burger before giving it to customer */
void *cashier(void *i){
    int id = *((int *)i);
    while (1){
        sem_wait(&customer_here);
        sem_post(&cashier_ready);
        printf("cashier[%d] accepts an order\n", id);
        sem_wait(&burger_ready);
        printf("cashier[%d] take a burger to customer\n", id);
        sem_post(&rack_available);

        //if all customers are served, let kill begin
        sem_wait(&leave_mutex);
        customer_left--;
        if (customer_left == 0)
            sem_post(&customer_none);
        sem_post(&leave_mutex);
    }
}

/* thread for customer
 * first tell cashier there comes new customer
 * then wait for cashier to ready*/
void *customer(void *i){
    int id = *((int*)i);
    printf("customer[%d] come.\n", id);
    sem_post(&customer_here);
    sem_wait(&cashier_ready);
}

/* main function
 * initialize everything and start all the threads
 * and kill threads when they are no longer useful
 * need 4 arguments: number of cooks, cashier, customer, rack*/
int main(int argc,char *argv[]){
    //deal with input arguments
    if (argc!=5) {
        printf("number of argument wrong!\n");
        return 1;
    }
	printf("%s, %s, %s, %s\n", argv[1], argv[2], argv[3], argv[4]);
    int cook_num = atoi(argv[1]);
    int cashier_num = atoi(argv[2]);
    int customer_num = atoi(argv[3]);
    int rack_num = atoi(argv[4]);
    if (cook_num==0 || cashier_num==0 || rack_num==0){
        printf("input format wrong!\n");
        return 1;
    }

    //initialize semaphore
    sem_init(&rack_available, 1, rack_num);
    sem_init(&burger_ready, 1, 0);
    sem_init(&cashier_ready, 1, 0);
    sem_init(&customer_here, 1, 0);
    sem_init(&customer_none, 1, 0);
    sem_init(&leave_mutex, 1, 1);
    customer_left = customer_num;

    //start threads
    pthread_t cook_threads[cook_num], cashier_threads[cashier_num], customer_threads[customer_num];
    int cook_ids[cook_num], cashier_ids[cashier_num], customer_ids[customer_num];
    int i, ret;
    for(i =0; i < cook_num; ++i){
        cook_ids[i] = i+1;
        ret = pthread_create(cook_threads+i, NULL, cook, (void*)&cook_ids[i]);
        if(ret!=0){
            printf("create cook thread %d failed\n", i);
        }
    }
    for(i =0; i < cashier_num; ++i){
        cashier_ids[i] = i+1;
        ret = pthread_create(cashier_threads+i, NULL, cashier, (void*)&cashier_ids[i]);
        if(ret!=0){
            printf("create cashier thread %d failed\n", i);
        }
    }
    for(i =0; i < customer_num; ++i){
        customer_ids[i] = i+1;
        ret = pthread_create(customer_threads+i, NULL, customer, (void*)&customer_ids[i]);
        if(ret!=0){
            printf("create customer thread %d failed\n", i);
        }
    }

    //after all customer served, kill all child thread
    sem_wait(&customer_none);
    for(i =0; i < cook_num; ++i)
        pthread_kill(cook_threads[i], SIGKILL);
    for(i =0; i < cashier_num; ++i)
        pthread_kill(cashier_threads[i], SIGKILL);
    for(i =0; i < cook_num; ++i)
        pthread_kill(customer_threads[i], SIGKILL);
    for(i =0; i < cook_num; ++i)
        pthread_join(cook_threads[i], NULL);
    for(i =0; i < cashier_num; ++i)
        pthread_join(cashier_threads[i], NULL);
    for(i =0; i < cook_num; ++i)
        pthread_join(customer_threads[i], NULL);


    return 0;
}
