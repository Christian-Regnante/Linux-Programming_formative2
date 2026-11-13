#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void die(const char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

static void close_or_die(int fd)
{
    if (close(fd) == -1) {
        die("close");
    }
}

static void child_exec(char *const command[], int input_fd, int output_fd,
                       int p12[2], int p23[2], int parent_output[2])
{
    if (dup2(input_fd, STDIN_FILENO) == -1) {
        perror("dup2 stdin");
        _exit(127);
    }
    if (dup2(output_fd, STDOUT_FILENO) == -1) {
        perror("dup2 stdout");
        _exit(127);
    }

    int fds[] = {p12[0], p12[1], p23[0], p23[1],
                 parent_output[0], parent_output[1]};
    for (size_t i = 0; i < sizeof(fds) / sizeof(fds[0]); ++i) {
        if (fds[i] != STDIN_FILENO && fds[i] != STDOUT_FILENO &&
            close(fds[i]) == -1 && errno != EBADF) {
            perror("close");
            _exit(127);
        }
    }
    execvp(command[0], command);
    perror(command[0]);
    _exit(127);
}

static void print_result_preview(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        die("open result for reading");
    }

    char buffer[4096];
    char *line = NULL;
    size_t line_capacity = 0;
    size_t line_length = 0;
    size_t line_count = 0;
    int preview_lines = 0;

    for (;;) {
        ssize_t bytes = read(fd, buffer, sizeof(buffer));
        if (bytes == -1) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            die("read result");
        }
        if (bytes == 0) {
            break;
        }

        for (ssize_t i = 0; i < bytes; ++i) {
            char ch = buffer[i];
            if (line_length == line_capacity) {
                size_t new_capacity = line_capacity == 0 ? 256 : line_capacity * 2;
                char *new_line = realloc(line, new_capacity);
                if (new_line == NULL) {
                    free(line);
                    close(fd);
                    die("realloc result line");
                }
                line = new_line;
                line_capacity = new_capacity;
            }
            line[line_length++] = ch;
            if (ch == '\n') {
                ++line_count;
                if (preview_lines < 5) {
                    if (write(STDOUT_FILENO, line, line_length) == -1) {
                        free(line);
                        close(fd);
                        die("write preview");
                    }
                    ++preview_lines;
                }
                line_length = 0;
            }
        }
    }
    if (line_length > 0) {
        ++line_count;
        if (preview_lines < 5) {
            if (write(STDOUT_FILENO, line, line_length) == -1) {
                free(line);
                close(fd);
                die("write preview");
            }
        }
    }
    free(line);
    close_or_die(fd);
    printf("Total lines: %zu\n", line_count);
}

int main(void)
{
    int p12[2], p23[2], parent_output[2];
    if (pipe(p12) == -1 || pipe(p23) == -1 || pipe(parent_output) == -1) {
        die("pipe");
    }

    char *stage1[] = {"cat", "/etc/passwd", NULL};
    char *stage2[] = {"cut", "-d:", "-f7", NULL};
    char *stage3[] = {"sort", "-u", NULL};
    pid_t children[3];

    children[0] = fork();
    if (children[0] == -1) {
        die("fork");
    }
    if (children[0] == 0) {
        child_exec(stage1, STDIN_FILENO, p12[1], p12, p23, parent_output);
    }

    children[1] = fork();
    if (children[1] == -1) {
        die("fork");
    }
    if (children[1] == 0) {
        child_exec(stage2, p12[0], p23[1], p12, p23, parent_output);
    }

    children[2] = fork();
    if (children[2] == -1) {
        die("fork");
    }
    if (children[2] == 0) {
        child_exec(stage3, p23[0], parent_output[1], p12, p23, parent_output);
    }

    close_or_die(p12[0]);
    close_or_die(p12[1]);
    close_or_die(p23[0]);
    close_or_die(p23[1]);
    close_or_die(parent_output[1]);

    int result_fd = open("result.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (result_fd == -1) {
        die("open result for writing");
    }

    char buffer[4096];
    for (;;) {
        ssize_t bytes = read(parent_output[0], buffer, sizeof(buffer));
        if (bytes == -1) {
            if (errno == EINTR) {
                continue;
            }
            close(result_fd);
            die("read pipeline output");
        }
        if (bytes == 0) {
            break;
        }
        ssize_t written = 0;
        while (written < bytes) {
            ssize_t n = write(result_fd, buffer + written, (size_t)(bytes - written));
            if (n == -1) {
                if (errno == EINTR) {
                    continue;
                }
                close(result_fd);
                die("write result");
            }
            written += n;
        }
    }
    close_or_die(parent_output[0]);
    close_or_die(result_fd);

    print_result_preview("result.txt");

    for (size_t i = 0; i < 3; ++i) {
        int status;
        if (waitpid(children[i], &status, 0) == -1) {
            die("waitpid");
        }
        if (WIFEXITED(status)) {
            printf("Child %ld exited with status %d\n",
                   (long)children[i], WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Child %ld terminated by signal %d\n",
                   (long)children[i], WTERMSIG(status));
        }
    }
    return EXIT_SUCCESS;
}
