#!/bin/sh
set -eu

output_dir=${1:-generated-logs}
mkdir -p "$output_dir"

i=1
while [ "$i" -le 12 ]; do
    size_mb=$((i + 1))
    file="$output_dir/log${i}.log"
    awk -v target="$size_mb" -v pattern="${PATTERN:-ERROR}" '
        BEGIN {
            line = "INFO request completed successfully with status 200\n"
            match_line = "ERROR request failed with status 500\n"
            target_bytes = target * 1024 * 1024
            bytes = 0
            line_number = 0
            while (bytes < target_bytes) {
                text = (line_number % 10 == 0 ? match_line : line)
                printf "%s", text
                bytes += length(text)
                line_number++
            }
        }
    ' /dev/null > "$file"
    i=$((i + 1))
done
