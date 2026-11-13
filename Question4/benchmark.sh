#!/bin/sh
set -eu

pattern=${1:-ERROR}
log_dir=${2:-generated-logs}
output_dir=${3:-benchmark-data}
mkdir -p "$output_dir"

set -- "$log_dir"/*.log
if [ "$#" -eq 0 ] || [ ! -e "$1" ]; then
    echo "No log files found; run ./generate_logs.sh first." >&2
    exit 1
fi

printf 'threads,run,elapsed_seconds,total_matches\n' \
    > "$output_dir/timings.csv"
for threads in 1 2 4 8; do
    for run in 1 2 3; do
        report="$output_dir/report-${threads}-${run}.txt"
        output=$(./logscan "$pattern" "$report" "$threads" "$@")
        elapsed=$(printf '%s\n' "$output" |
            awk '/Multi-threaded time:/ {print $3}')
        total=$(printf '%s\n' "$output" |
            awk '/Total matching lines across/ {print $NF; exit}')
        printf '%s,%s,%s,%s\n' "$threads" "$run" "$elapsed" "$total" \
            >> "$output_dir/timings.csv"
    done
done

for file in "$@"; do
    grep -c "$pattern" "$file"
done > "$output_dir/grep-counts.txt"
