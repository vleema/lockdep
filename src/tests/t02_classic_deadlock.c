#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

void* thread1_func(void* arg __attribute__((unused))) {
    printf("Thread 1: Acquiring mutex1\n");
    pthread_mutex_lock(&mutex1);
    printf("Thread 1: Got mutex1, sleeping...\n");

    sleep(1);

    printf("Thread 1: Trying to acquire mutex2\n");
    pthread_mutex_lock(&mutex2);
    printf("Thread 1: Got mutex2\n");

    pthread_mutex_unlock(&mutex2);
    pthread_mutex_unlock(&mutex1);

    return NULL;
}

void* thread2_func(void* arg __attribute__((unused))) {
    printf("Thread 2: Acquiring mutex2\n");
    pthread_mutex_lock(&mutex2);
    printf("Thread 2: Got mutex2, sleeping...\n");

    sleep(1);

    printf("Thread 2: Trying to acquire mutex1\n");
    pthread_mutex_lock(&mutex1);
    printf("Thread 2: Got mutex1\n");

    pthread_mutex_unlock(&mutex1);
    pthread_mutex_unlock(&mutex2);

    return NULL;
}

int main() {
    pthread_t t1, t2;

    printf("Starting deadlock test (classic AB-BA pattern)\n");

    pthread_create(&t1, NULL, thread1_func, NULL);
    pthread_create(&t2, NULL, thread2_func, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Test completed\n");
    return 0;
}
