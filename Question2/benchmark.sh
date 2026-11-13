#!/bin/sh
set -eu

input=${1:-big.bin}
output_dir=${2:-benchmark-data}
mkdir -p "$output_dir"

if [ ! -f "$input" ]; then
    dd if=/dev/urandom of="$input" bs=1M count=100 status=progress
fi

printf 'version,buffer_bytes,run,elapsed_seconds\n' > "$output_dir/timings.csv"
for buffer_size in 512 4096 65536; do
    for run in 1 2 3; do
        sys_output="$output_dir/sys-${buffer_size}-${run}.bin"
        stdio_output="$output_dir/stdio-${buffer_size}-${run}.bin"
        sys_time=$(./copy_sys "$input" "$sys_output" "$buffer_size" |
            awk '/Elapsed time:/ {print $3}')
        stdio_time=$(./copy_stdio "$input" "$stdio_output" "$buffer_size" |
            awk '/Elapsed time:/ {print $3}')
        cmp "$input" "$sys_output"
        cmp "$input" "$stdio_output"
        printf 'copy_sys,%s,%s,%s\n' "$buffer_size" "$run" "$sys_time" \
            >> "$output_dir/timings.csv"
        printf 'copy_stdio,%s,%s,%s\n' "$buffer_size" "$run" "$stdio_time" \
            >> "$output_dir/timings.csv"
    done
done

strace -c ./copy_sys "$input" "$output_dir/strace-sys.bin" 4096 \
    2> "$output_dir/strace-copy-sys.txt"
strace -c ./copy_stdio "$input" "$output_dir/strace-stdio.bin" 4096 \
    2> "$output_dir/strace-copy-stdio.txt"
