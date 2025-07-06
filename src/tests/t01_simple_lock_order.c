#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

int main() {
    printf("Starting simple lock ordering test\n");

    printf("Acquiring mutex1\n");
    pthread_mutex_lock(&mutex1);

    printf("Acquiring mutex2 (creating dependency: mutex1 -> mutex2)\n");
    pthread_mutex_lock(&mutex2);

    printf("Releasing mutex2\n");
    pthread_mutex_unlock(&mutex2);

    printf("Releasing mutex1\n");
    pthread_mutex_unlock(&mutex1);

    printf("Test completed\n");
    return 0;
}
