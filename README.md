# Linux Programming Formative Assignment

This repository contains four independent POSIX C projects covering process pipelines, file-I/O performance, pthread synchronization, and a thread-pool log scanner. The assessment goals and complete requirements are in `AssessmentGoals_and_Instructions.md` and `Assignment-Questions_and_Rubric/`.

## Project layout

Each question is isolated in its own directory:

- `Question1/` — `fork()`/`execvp()` pipeline and `strace` analysis.
- `Question2/` — syscall-based and stdio-based file copying benchmarks.
- `Question3/` — single-threaded and eight-thread perfect-number search.
- `Question4/` — mutex-protected worker-pool log scanner.

Final analytical reports are written as `.docx` files in the relevant question directory. Generated binaries, traces, benchmark data, and large input files should remain question-local.

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

The reproducible trace and benchmark workflows are in the scripts within the corresponding question directory. See `IMPLEMENTATION_PLAN.md` for the acceptance checks and required report contents.

## Generate reports

After collecting the evidence with the question-local scripts, create or refresh all four analytical reports with:

```sh
python3 -m venv /tmp/formative-report-venv
/tmp/formative-report-venv/bin/pip install python-docx
/tmp/formative-report-venv/bin/python generate_reports.py
```

The generated files are `Question1/report.docx` through `Question4/report.docx`.
