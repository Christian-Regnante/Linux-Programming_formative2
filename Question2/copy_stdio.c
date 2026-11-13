#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

static double seconds_between(const struct timespec *start,
                              const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000000.0;
}

static int parse_size(const char *text, size_t *value)
{
    char *end = NULL;
    errno = 0;
    unsigned long long parsed = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed == 0 ||
        parsed > (unsigned long long)SIZE_MAX) {
        return -1;
    }
    *value = (size_t)parsed;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s source destination buffer_size\n", argv[0]);
        return EXIT_FAILURE;
    }

    size_t buffer_size;
    if (parse_size(argv[3], &buffer_size) != 0) {
        fprintf(stderr, "Invalid buffer size: %s\n", argv[3]);
        return EXIT_FAILURE;
    }

    FILE *source = fopen(argv[1], "rb");
    if (source == NULL) {
        perror("fopen source");
        return EXIT_FAILURE;
    }
    FILE *destination = fopen(argv[2], "wb");
    if (destination == NULL) {
        perror("fopen destination");
        fclose(source);
        return EXIT_FAILURE;
    }

    unsigned char *buffer = malloc(buffer_size);
    if (buffer == NULL) {
        perror("malloc");
        fclose(source);
        fclose(destination);
        return EXIT_FAILURE;
    }

    struct timespec start, end;
    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        perror("clock_gettime");
        free(buffer);
        fclose(source);
        fclose(destination);
        return EXIT_FAILURE;
    }

    int failed = 0;
    for (;;) {
        size_t bytes = fread(buffer, 1, buffer_size, source);
        if (bytes > 0 && fwrite(buffer, 1, bytes, destination) != bytes) {
            perror("fwrite");
            failed = 1;
            break;
        }
        if (bytes < buffer_size) {
            if (ferror(source)) {
                perror("fread");
                failed = 1;
            }
            break;
        }
    }
    if (clock_gettime(CLOCK_MONOTONIC, &end) == -1) {
        perror("clock_gettime");
        failed = 1;
    }
    if (fclose(source) == EOF) {
        perror("fclose source");
        failed = 1;
    }
    if (fclose(destination) == EOF) {
        perror("fclose destination");
        failed = 1;
    }
    free(buffer);

    if (!failed) {
        printf("Elapsed time: %.9f seconds\n", seconds_between(&start, &end));
    }
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
