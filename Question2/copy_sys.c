#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

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

    int source = open(argv[1], O_RDONLY);
    if (source == -1) {
        perror("open source");
        return EXIT_FAILURE;
    }
    int destination = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (destination == -1) {
        perror("open destination");
        close(source);
        return EXIT_FAILURE;
    }

    char *buffer = malloc(buffer_size);
    if (buffer == NULL) {
        perror("malloc");
        close(source);
        close(destination);
        return EXIT_FAILURE;
    }

    struct timespec start, end;
    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        perror("clock_gettime");
        free(buffer);
        close(source);
        close(destination);
        return EXIT_FAILURE;
    }

    int failed = 0;
    for (;;) {
        ssize_t bytes = read(source, buffer, buffer_size);
        if (bytes == 0) {
            break;
        }
        if (bytes == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            failed = 1;
            break;
        }
        ssize_t offset = 0;
        while (offset < bytes) {
            ssize_t written = write(destination, buffer + offset,
                                     (size_t)(bytes - offset));
            if (written == -1) {
                if (errno == EINTR) {
                    continue;
                }
                perror("write");
                failed = 1;
                break;
            }
            offset += written;
        }
        if (failed) {
            break;
        }
    }
    if (clock_gettime(CLOCK_MONOTONIC, &end) == -1) {
        perror("clock_gettime");
        failed = 1;
    }
    if (close(source) == -1) {
        perror("close source");
        failed = 1;
    }
    if (close(destination) == -1) {
        perror("close destination");
        failed = 1;
    }
    free(buffer);

    if (!failed) {
        printf("Elapsed time: %.9f seconds\n", seconds_between(&start, &end));
    }
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
