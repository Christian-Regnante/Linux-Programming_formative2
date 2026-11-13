#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LIMIT 100000
#define THREAD_COUNT 8
#define SEGMENT_SIZE (LIMIT / THREAD_COUNT)
#define MAX_PERFECT_NUMBERS 32

typedef struct {
    int start;
    int end;
    int id;
} worker_arg;

typedef struct {
    int numbers[MAX_PERFECT_NUMBERS];
    size_t count;
    pthread_mutex_t mutex;
} shared_results;

static int is_perfect(int number)
{
    if (number < 2) {
        return 0;
    }
    int sum = 1;
    for (int divisor = 2; divisor <= number / divisor; ++divisor) {
        if (number % divisor == 0) {
            sum += divisor;
            int pair = number / divisor;
            if (pair != divisor) {
                sum += pair;
            }
            if (sum > number) {
                return 0;
            }
        }
    }
    return sum == number;
}

static size_t find_range(int start, int end, int *numbers, size_t capacity)
{
    size_t count = 0;
    for (int number = start; number <= end; ++number) {
        if (is_perfect(number) && count < capacity) {
            numbers[count++] = number;
        }
    }
    return count;
}

typedef struct {
    worker_arg range;
    shared_results *results;
} thread_context;

static void *search_worker(void *argument)
{
    thread_context *context = argument;
    int local_numbers[MAX_PERFECT_NUMBERS];
    size_t local_count = find_range(context->range.start, context->range.end,
                                    local_numbers, MAX_PERFECT_NUMBERS);

    int lock_result = pthread_mutex_lock(&context->results->mutex);
    if (lock_result != 0) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(lock_result));
        return NULL;
    }
    for (size_t i = 0; i < local_count; ++i) {
        if (context->results->count < MAX_PERFECT_NUMBERS) {
            context->results->numbers[context->results->count++] =
                local_numbers[i];
        }
    }
    lock_result = pthread_mutex_unlock(&context->results->mutex);
    if (lock_result != 0) {
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(lock_result));
    }
    return NULL;
}

static int compare_ints(const void *left, const void *right)
{
    int a = *(const int *)left;
    int b = *(const int *)right;
    return (a > b) - (a < b);
}

static double elapsed_seconds(const struct timespec *start,
                              const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000000.0;
}

static size_t run_single_threaded(int *numbers)
{
    return find_range(1, LIMIT, numbers, MAX_PERFECT_NUMBERS);
}

int main(void)
{
    int single_numbers[MAX_PERFECT_NUMBERS];
    struct timespec single_start, single_end;
    if (clock_gettime(CLOCK_MONOTONIC, &single_start) == -1) {
        perror("clock_gettime");
        return EXIT_FAILURE;
    }
    size_t single_count = run_single_threaded(single_numbers);
    if (clock_gettime(CLOCK_MONOTONIC, &single_end) == -1) {
        perror("clock_gettime");
        return EXIT_FAILURE;
    }

    shared_results results = {0};
    int mutex_result = pthread_mutex_init(&results.mutex, NULL);
    if (mutex_result != 0) {
        fprintf(stderr, "pthread_mutex_init: %s\n", strerror(mutex_result));
        return EXIT_FAILURE;
    }
    pthread_t threads[THREAD_COUNT];
    thread_context contexts[THREAD_COUNT];
    size_t created = 0;
    struct timespec multi_start, multi_end;
    if (clock_gettime(CLOCK_MONOTONIC, &multi_start) == -1) {
        perror("clock_gettime");
        pthread_mutex_destroy(&results.mutex);
        return EXIT_FAILURE;
    }

    int failed = 0;
    for (int i = 0; i < THREAD_COUNT; ++i) {
        contexts[i].range.start = i * SEGMENT_SIZE + 1;
        contexts[i].range.end = (i + 1) * SEGMENT_SIZE;
        contexts[i].range.id = i;
        contexts[i].results = &results;
        int create_result = pthread_create(&threads[i], NULL, search_worker,
                                           &contexts[i]);
        if (create_result != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(create_result));
            failed = 1;
            break;
        }
        ++created;
    }
    for (size_t i = 0; i < created; ++i) {
        int join_result = pthread_join(threads[i], NULL);
        if (join_result != 0) {
            fprintf(stderr, "pthread_join: %s\n", strerror(join_result));
            failed = 1;
        }
    }
    if (clock_gettime(CLOCK_MONOTONIC, &multi_end) == -1) {
        perror("clock_gettime");
        failed = 1;
    }
    pthread_mutex_destroy(&results.mutex);
    if (failed || created != THREAD_COUNT) {
        return EXIT_FAILURE;
    }

    qsort(single_numbers, single_count, sizeof(single_numbers[0]), compare_ints);
    qsort(results.numbers, results.count, sizeof(results.numbers[0]), compare_ints);
    if (single_count != results.count) {
        fprintf(stderr, "Single-threaded and multi-threaded counts differ\n");
        return EXIT_FAILURE;
    }
    for (size_t i = 0; i < single_count; ++i) {
        if (single_numbers[i] != results.numbers[i]) {
            fprintf(stderr, "Single-threaded and multi-threaded results differ\n");
            return EXIT_FAILURE;
        }
    }
    printf("Perfect numbers found:");
    for (size_t i = 0; i < results.count; ++i) {
        printf(" %d", results.numbers[i]);
    }
    putchar('\n');
    printf("The synchronized total number of perfect numbers between 1 and 100000 is %zu\n",
           results.count);
    printf("Single-threaded time: %.9f seconds\n",
           elapsed_seconds(&single_start, &single_end));
    printf("Multi-threaded time:  %.9f seconds\n",
           elapsed_seconds(&multi_start, &multi_end));
    return EXIT_SUCCESS;
}
