# Linux Programming Formative Assignment

This repository contains four independent POSIX C projects covering process pipelines, file-I/O performance, pthread synchronization, and a thread-pool log scanner. The assessment goals and complete requirements are in `AssessmentGoals_and_Instructions.md` and `Assignment-Questions_and_Rubric/`.

## Project layout

Each question is isolated in its own directory:

- `Question1/` — `fork()`/`execvp()` pipeline and `strace` analysis.
- `Question2/` — syscall-based and stdio-based file copying benchmarks.
- `Question3/` — single-threaded and eight-thread perfect-number search.
- `Question4/` — mutex-protected worker-pool log scanner.

## Compile

```sh
gcc -Wall -Wextra -Wpedantic -std=c11 -O2 -o Question1/pipeline Question1/pipeline.c
gcc -Wall -Wextra -Wpedantic -std=c11 -O2 -o Question2/copy_sys Question2/copy_sys.c
gcc -Wall -Wextra -Wpedantic -std=c11 -O2 -o Question2/copy_stdio Question2/copy_stdio.c
gcc -Wall -Wextra -Wpedantic -std=c11 -O2 -pthread -o Question3/perfect_numbers Question3/perfect_numbers.c
gcc -Wall -Wextra -Wpedantic -std=c11 -O2 -pthread -o Question4/logscan Question4/logscan.c
```

## Focused checks

```sh
(cd Question1 && ./pipeline)
(cd Question2 && ./copy_sys source.bin destination.bin 4096 && cmp source.bin destination.bin)
(cd Question2 && ./copy_stdio source.bin destination.bin 4096 && cmp source.bin destination.bin)
(cd Question3 && ./perfect_numbers)
(cd Question4 && ./logscan pattern report.txt 4 file1.log file2.log)
```

## Reproducibility scripts

The question directories contain shell scripts that automate the required evidence collection:

| Script | Purpose |
|---|---|
| `Question1/run_trace.sh` | Compiles the pipeline and captures full and filtered `strace` logs. |
| `Question2/benchmark.sh` | Generates the 100 MiB input when needed, runs all buffer-size repetitions, verifies copies with `cmp`, and captures `strace -c` summaries. |
| `Question3/run_check.sh` | Compiles and runs the perfect-number program for a focused correctness check. |
| `Question4/generate_logs.sh` | Generates twelve differently sized log files containing the benchmark pattern. |
| `Question4/benchmark.sh` | Runs the 1-, 2-, 4-, and 8-thread tests three times and records timing and `grep` verification data. |

Run scripts from their own question directory, for example:

```sh
cd Question2
./benchmark.sh big.bin benchmark-data
```

