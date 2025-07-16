#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * This test implements the classic Dining Philosophers problem
 *
 * Five philosophers sit around a table with five forks (one between each pair).
 * Each philosopher needs two forks to eat (the one on their left and right).
 * If all philosophers pick up the fork on their left at the same time,
 * none of them can pick up their right fork, resulting in deadlock.
 */

#define SHALL_LOCK 1
#define NUM_PHILOSOPHERS 5
#define THINKING 0
#define HUNGRY 1
#define EATING 2

pthread_mutex_t forks[NUM_PHILOSOPHERS];
int state[NUM_PHILOSOPHERS];
pthread_t philosophers[NUM_PHILOSOPHERS];

void print_states()
{
    printf("Philosophers: ");
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        printf("%d:", i);
        switch (state[i]) {
        case THINKING:
            printf("🤔 ");
            break;
        case HUNGRY:
            printf("😋 ");
            break;
        case EATING:
            printf("🍝 ");
            break;
        }
    }
    printf("\n");
    fflush(stdout);
}

void* philosopher(void* arg)
{
    int id = *(int*)arg;
    int left_fork = id;
    int right_fork = (id + 1) % NUM_PHILOSOPHERS;

    // We want most philosophers to pick up left fork first, but
    // let the last philosopher pick up right fork first to avoid deadlock
    int first_fork, second_fork;

    if (SHALL_LOCK){
        first_fork = left_fork;
        second_fork = right_fork;
    } else {
        if (id == NUM_PHILOSOPHERS - 1) {
            // Last philosopher picks up right fork first
            first_fork = right_fork;
            second_fork = left_fork;
        } else {
            // Other philosophers pick up left fork first
            first_fork = left_fork;
            second_fork = right_fork;
        }
    }

    // Philosopher lifecycle
    for (int cycle = 0; cycle < 3; cycle++) {
        // Think for a while
        state[id] = THINKING;
        printf("Philosopher %d is thinking\n", id);
        fflush(stdout);
        print_states();
        sleep(rand() % 2 + 1);

        // Get hungry
        state[id] = HUNGRY;
        printf("Philosopher %d is hungry\n", id);
        fflush(stdout);
        print_states();

        // Pick up first fork
        printf("Philosopher %d is trying to pick up fork %d\n", id, first_fork);
        fflush(stdout);
        pthread_mutex_lock(&forks[first_fork]);
        printf("Philosopher %d picked up fork %d\n", id, first_fork);
        fflush(stdout);

        // Small delay to make deadlock more likely if we're not avoiding it
        usleep(500000);

        // Pick up second fork
        printf("Philosopher %d is trying to pick up fork %d\n", id, second_fork);
        fflush(stdout);
        pthread_mutex_lock(&forks[second_fork]);
        printf("Philosopher %d picked up fork %d\n", id, second_fork);
        fflush(stdout);

        // Eat
        state[id] = EATING;
        printf("Philosopher %d is eating\n", id);
        fflush(stdout);
        print_states();
        sleep(rand() % 2 + 1);

        // Put down forks
        pthread_mutex_unlock(&forks[second_fork]);
        printf("Philosopher %d put down fork %d\n", id, second_fork);
        fflush(stdout);
        pthread_mutex_unlock(&forks[first_fork]);
        printf("Philosopher %d put down fork %d\n", id, first_fork);
        fflush(stdout);
    }

    printf("Philosopher %d is done dining\n", id);
    fflush(stdout);
    return NULL;
}

int main()
{
    int ids[NUM_PHILOSOPHERS];
    srand(time(NULL));

    printf("Dining Philosophers Problem - Deadlock Demonstration\n");
    fflush(stdout);
    printf("===================================================\n\n");
    fflush(stdout);

    // Initialize mutexes (forks)
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_mutex_init(&forks[i], NULL);
        state[i] = THINKING;
    }

    // Create philosopher threads
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        ids[i] = i;
        pthread_create(&philosophers[i], NULL, philosopher, &ids[i]);
    }

    // Wait for all philosophers to finish
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_join(philosophers[i], NULL);
    }

    // Destroy the mutexes
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_mutex_destroy(&forks[i]);
    }

    printf("\nAll philosophers have finished dining.\n");
    printf("Note: This implementation avoids deadlock by having one "
           "philosopher\n");
    printf("pick up forks in a different order. To see a deadlock, modify the\n");
    printf("philosopher function to always pick up the left fork first.\n");
    fflush(stdout);

    return 0;
}
