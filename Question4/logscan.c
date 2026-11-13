#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

typedef struct {
    const char *pattern;
    const char *report_path;
    char **files;
    size_t file_count;
    size_t next_file;
    size_t total_matches;
    size_t total_lines;
    FILE *report;
    pthread_mutex_t queue_mutex;
    pthread_mutex_t report_mutex;
    pthread_mutex_t totals_mutex;
} scan_context;

typedef struct {
    scan_context *context;
    size_t id;
} worker_context;

static double elapsed_seconds(const struct timespec *start,
                              const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000000.0;
}

static int scan_file(const char *pattern, const char *path,
                     size_t *matching_lines, size_t *total_lines)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        perror(path);
        return -1;
    }
    char line[8192];
    size_t matches = 0;
    size_t total = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        ++total;
        if (strstr(line, pattern) != NULL) {
            ++matches;
        }
    }
    int failed = ferror(file);
    if (failed) {
        perror(path);
    }
    if (fclose(file) == EOF) {
        perror(path);
        failed = 1;
    }
    if (failed) {
        return -1;
    }
    *matching_lines = matches;
    *total_lines = total;
    return 0;
}

static int claim_file(scan_context *context, size_t *index)
{
    int result = pthread_mutex_lock(&context->queue_mutex);
    if (result != 0) {
        fprintf(stderr, "pthread_mutex_lock queue: %s\n", strerror(result));
        return -1;
    }
    if (context->next_file >= context->file_count) {
        *index = context->file_count;
    } else {
        *index = context->next_file++;
    }
    result = pthread_mutex_unlock(&context->queue_mutex);
    if (result != 0) {
        fprintf(stderr, "pthread_mutex_unlock queue: %s\n", strerror(result));
        return -1;
    }
    return 0;
}

static void *worker_main(void *argument)
{
    worker_context *worker = argument;
    scan_context *context = worker->context;
    for (;;) {
        size_t index;
        if (claim_file(context, &index) != 0 || index == context->file_count) {
            return NULL;
        }
        size_t matches = 0;
        size_t total = 0;
        if (scan_file(context->pattern, context->files[index], &matches,
                      &total) != 0) {
            continue;
        }

        int result = pthread_mutex_lock(&context->report_mutex);
        if (result != 0) {
            fprintf(stderr, "pthread_mutex_lock report: %s\n", strerror(result));
            return NULL;
        }
        fprintf(context->report, "%s: %zu matching lines out of %zu "
                "(thread %zu)\n", context->files[index], matches, total,
                worker->id);
        result = pthread_mutex_unlock(&context->report_mutex);
        if (result != 0) {
            fprintf(stderr, "pthread_mutex_unlock report: %s\n", strerror(result));
            return NULL;
        }

        result = pthread_mutex_lock(&context->totals_mutex);
        if (result != 0) {
            fprintf(stderr, "pthread_mutex_lock totals: %s\n", strerror(result));
            return NULL;
        }
        context->total_matches += matches;
        context->total_lines += total;
        result = pthread_mutex_unlock(&context->totals_mutex);
        if (result != 0) {
            fprintf(stderr, "pthread_mutex_unlock totals: %s\n", strerror(result));
            return NULL;
        }
    }
}

static int run_single_threaded(const char *pattern, char **files,
                               size_t file_count, size_t *matches,
                               size_t *total_lines)
{
    *matches = 0;
    *total_lines = 0;
    for (size_t i = 0; i < file_count; ++i) {
        size_t file_matches = 0;
        size_t file_lines = 0;
        if (scan_file(pattern, files[i], &file_matches, &file_lines) == 0) {
            *matches += file_matches;
            *total_lines += file_lines;
        }
    }
    return 0;
}

static void destroy_context_mutexes(scan_context *context)
{
    pthread_mutex_destroy(&context->queue_mutex);
    pthread_mutex_destroy(&context->report_mutex);
    pthread_mutex_destroy(&context->totals_mutex);
}

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "Usage: %s pattern report.txt number_of_threads "
                "file1.log ...\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *end = NULL;
    errno = 0;
    unsigned long parsed_threads = strtoul(argv[3], &end, 10);
    if (errno != 0 || end == argv[3] || *end != '\0' ||
        parsed_threads == 0 ||
        parsed_threads > (unsigned long)SIZE_MAX) {
        fprintf(stderr, "Invalid thread count: %s\n", argv[3]);
        return EXIT_FAILURE;
    }
    size_t thread_count = (size_t)parsed_threads;
    size_t file_count = (size_t)(argc - 4);

    size_t single_matches, single_lines;
    struct timespec single_start, single_end;
    if (clock_gettime(CLOCK_MONOTONIC, &single_start) == -1) {
        perror("clock_gettime");
        return EXIT_FAILURE;
    }
    run_single_threaded(argv[1], &argv[4], file_count, &single_matches,
                        &single_lines);
    if (clock_gettime(CLOCK_MONOTONIC, &single_end) == -1) {
        perror("clock_gettime");
        return EXIT_FAILURE;
    }

    FILE *report = fopen(argv[2], "w");
    if (report == NULL) {
        perror("fopen report");
        return EXIT_FAILURE;
    }
    scan_context context = {
        .pattern = argv[1],
        .report_path = argv[2],
        .files = &argv[4],
        .file_count = file_count,
        .next_file = 0,
        .total_matches = 0,
        .total_lines = 0,
        .report = report
    };
    int result = pthread_mutex_init(&context.queue_mutex, NULL);
    if (result != 0) {
        fprintf(stderr, "pthread_mutex_init queue: %s\n", strerror(result));
        fclose(report);
        return EXIT_FAILURE;
    }
    result = pthread_mutex_init(&context.report_mutex, NULL);
    if (result != 0) {
        fprintf(stderr, "pthread_mutex_init report: %s\n", strerror(result));
        pthread_mutex_destroy(&context.queue_mutex);
        fclose(report);
        return EXIT_FAILURE;
    }
    result = pthread_mutex_init(&context.totals_mutex, NULL);
    if (result != 0) {
        fprintf(stderr, "pthread_mutex_init totals: %s\n", strerror(result));
        pthread_mutex_destroy(&context.queue_mutex);
        pthread_mutex_destroy(&context.report_mutex);
        fclose(report);
        return EXIT_FAILURE;
    }

    pthread_t *threads = calloc(thread_count, sizeof(*threads));
    worker_context *workers = calloc(thread_count, sizeof(*workers));
    if (threads == NULL || workers == NULL) {
        perror("calloc");
        free(threads);
        free(workers);
        destroy_context_mutexes(&context);
        fclose(report);
        return EXIT_FAILURE;
    }

    struct timespec multi_start, multi_end;
    if (clock_gettime(CLOCK_MONOTONIC, &multi_start) == -1) {
        perror("clock_gettime");
        free(threads);
        free(workers);
        destroy_context_mutexes(&context);
        fclose(report);
        return EXIT_FAILURE;
    }
    size_t created = 0;
    int failed = 0;
    for (size_t i = 0; i < thread_count; ++i) {
        workers[i].context = &context;
        workers[i].id = i;
        result = pthread_create(&threads[i], NULL, worker_main, &workers[i]);
        if (result != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(result));
            failed = 1;
            break;
        }
        ++created;
    }
    for (size_t i = 0; i < created; ++i) {
        result = pthread_join(threads[i], NULL);
        if (result != 0) {
            fprintf(stderr, "pthread_join: %s\n", strerror(result));
            failed = 1;
        }
    }
    if (clock_gettime(CLOCK_MONOTONIC, &multi_end) == -1) {
        perror("clock_gettime");
        failed = 1;
    }

    if (!failed) {
        fprintf(report, "Total matching lines across %zu files: %zu\n",
                file_count, context.total_matches);
        if (fclose(report) == EOF) {
            perror("fclose report");
            failed = 1;
        } else {
            printf("Total matching lines across %zu files: %zu\n",
                   file_count, context.total_matches);
            printf("Single-threaded time: %.9f seconds\n",
                   elapsed_seconds(&single_start, &single_end));
            printf("Multi-threaded time:  %.9f seconds\n",
                   elapsed_seconds(&multi_start, &multi_end));
        }
    } else {
        fclose(report);
    }
    free(threads);
    free(workers);
    destroy_context_mutexes(&context);
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
