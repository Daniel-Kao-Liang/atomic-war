#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>

// 設定實驗參數
#define NUM_THREADS 8           // 執行緒數量 (建議設為你的 CPU 核心數)
#define ITERATIONS  10000000    // 每個執行緒累加的次數 (總數 = Threads * Iterations)

// 共用變數
long long counter_unsafe = 0;
long long counter_mutex = 0;
long long counter_cas = 0;

pthread_mutex_t lock;

// ==========================================
// 1. Unsafe Worker: 完全沒保護，速度最快但結果會錯
// ==========================================
void* worker_unsafe(void* arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        counter_unsafe++; // Race Condition 發生地
    }
    return NULL;
}

// ==========================================
// 2. Mutex Worker: 傳統鎖，結果正確但稍慢
// ==========================================
void* worker_mutex(void* arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&lock);
        counter_mutex++;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

// ==========================================
// 3. CAS Worker (Lock-free): 使用 Compare and Swap
// 對應講稿中 "Atomic Counter Addition" 的實作 [cite: 183-189]
// ==========================================
void* worker_cas(void* arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        long long old_val, new_val;
        do {
            // A. 讀取當前值 (Load)
            old_val = counter_cas;
            
            // B. 計算新值 (Add)
            new_val = old_val + 1;
            
            // C. 比較並交換 (Compare and Swap)
            // __sync_bool_compare_and_swap 是 GCC 內建的原子指令
            // 邏輯：如果 &counter_cas 還等於 old_val，就把它改成 new_val 並回傳 true
            //      否則回傳 false (代表被別人改過了)，迴圈重跑
        } while (!__sync_bool_compare_and_swap(&counter_cas, old_val, new_val));
    }
    return NULL;
}

// 計時工具函式
double get_time_diff(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

void run_test(const char* name, void* (*func)(void*), long long* counter) {
    pthread_t threads[NUM_THREADS];
    struct timespec start, end;
    
    printf("正在測試 %-10s ... ", name);
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
    
    printf("耗時: %.4f 秒 | 結果: %lld / %lld ", time_taken, *counter, expected);    
    if (*counter == expected) {
        printf("[✅ 正確]\n");
    } else {
        printf("[❌ 錯誤] 遺失了 %lld 次更新\n", expected - *counter);
    }
}

int main() {
    pthread_mutex_init(&lock, NULL);

    printf("=== 原子大戰: Mutex vs CAS (Lock-free) ===\n");
    printf("執行緒: %d, 每個累加: %d 次\n\n", NUM_THREADS, ITERATIONS);

    run_test("Unsafe", worker_unsafe, &counter_unsafe);

    run_test("CAS (Lock-free)", worker_cas, &counter_cas);

    run_test("Mutex", worker_mutex, &counter_mutex);
    pthread_mutex_destroy(&lock);
    return 0;
}