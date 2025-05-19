// Evaluate swapping and responsiveness under extreme memory pressure conditions.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define THREADS_COUNT 5
#define ALLOC_SIZE_MB 200 // each thread allocates 200MB
#define RUN_DURATION 120  // Run test for 120 seconds

void *high_pressure_task(void *arg) {
    char *memory = malloc(ALLOC_SIZE_MB * 1024 * 1024);
    if (!memory) {
        perror("Thread memory allocation failed");
        return NULL;
    }

    memset(memory, 0xFF, ALLOC_SIZE_MB * 1024 * 1024);
    printf("[Thread %ld] allocated and initialized %d MB\n", pthread_self(), ALLOC_SIZE_MB);

    for(int t=0; t < RUN_DURATION; t+=2){
        // Keep accessing memory
        for(int i = 0; i < ALLOC_SIZE_MB * 1024 * 1024; i+=4096)
            memory[i] ^= 0xFF;  // flip bits to force active accesses

        printf("[Thread %ld] active access at %d seconds.\n", pthread_self(), t);
        sleep(2);
    }

    free(memory);
    return NULL;
}

int main() {
    pthread_t threads[THREADS_COUNT];

    printf("Starting Extreme Memory Pressure and Swapping Stress Test...\n");

    // Launch multiple heavy memory-consumption threads
    for(int i = 0; i < THREADS_COUNT; ++i){
        if(pthread_create(&threads[i], NULL, high_pressure_task, NULL)) {
            perror("Failed to create thread");
            exit(1);
        }
        sleep(1);
    }

    printf("All threads started. Monitor kernel indicator and user-space swapping behavior manually for %d seconds.\n", RUN_DURATION);

    // Regularly check kernel memory pressure
    for(int t=0; t < RUN_DURATION; t+=5){
        FILE *fp = fopen("/proc/mem_pressure", "r");
        if(fp){
            char buffer[256];
            fgets(buffer, sizeof(buffer), fp);
            printf("[Memory pressure at %d sec]: %s", t, buffer);
            fclose(fp);
        } else {
            printf("Failed opening kernel memory pressure indication file.\n");
        }
        sleep(5);
    }

    // Join threads
    for(int i=0; i<THREADS_COUNT; ++i)
        pthread_join(threads[i], NULL);

    printf("Test complete. Check logs and user-space swapping correctness manually.\n");
    return 0;
}

