#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>

#define NUM_THREADS 8
#define ITERATIONS  10000000 

long long counter_unsafe = 0;
long long counter_mutex = 0;
long long counter_cas = 0;
long long counter_tas = 0;
volatile int spinlock_flag = 0;

pthread_mutex_t lock;


void* worker_unsafe(void* arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        counter_unsafe++; // Race Conditio
    }
    return NULL;
}

void* worker_mutex(void* arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&lock);
        counter_mutex++;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

void* worker_cas(void* arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        long long old_val, new_val;
        do {
            old_val = counter_cas;
            new_val = old_val + 1;
        } while (!__sync_bool_compare_and_swap(&counter_cas, old_val, new_val));
    }
    return NULL;
}

void* worker_tas(void* arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        while (__sync_lock_test_and_set(&spinlock_flag, 1)) {}
        counter_tas++;
        __sync_lock_release(&spinlock_flag);
    }
    return NULL;
}

double get_time_diff(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

void run_test(const char* name, void* (*func)(void*), long long* counter) {
    pthread_t threads[NUM_THREADS];
    struct timespec start, end;
    
    printf("testing %-10s ... ", name);
    fflush(stdout);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, func, NULL);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double time_taken = get_time_diff(start, end);
    long long expected = (long long)NUM_THREADS * ITERATIONS;
    
    printf("time: %.4f s | consequence: %lld / %lld ", time_taken, *counter, expected);    
    if (*counter == expected) {
        printf("[✅ correct]\n");
    } else {
        printf("[❌ WA] loss %lld time renew\n", expected - *counter);
    }
}

int main() {
    pthread_mutex_init(&lock, NULL);

    printf("=== atomic_war: NoLock vs Mutex vs TAS vs CAS ===\n");
    printf("thread: %d, everyone: %d times\n\n", NUM_THREADS, ITERATIONS);

    run_test("Unsafe", worker_unsafe, &counter_unsafe);
    run_test("Mutex", worker_mutex, &counter_mutex);
    run_test("TAS", worker_tas, &counter_mutex);
    run_test("CAS (Lock-free)", worker_cas, &counter_cas);

    pthread_mutex_destroy(&lock);
    return 0;
}