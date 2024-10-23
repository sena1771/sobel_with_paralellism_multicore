#!/bin/bash

# Array of thread counts
thread_counts=(2 4 8 16 32)

# Loop over each thread count
for threads in "${thread_counts[@]}"; do
    echo "Running Cachegrind analysis with $threads threads..."

    # Set output file names
    cachegrind_out="cachegrind_out_${threads}"
    report_file="cachegrind_report_${threads}.txt"

    # Run Cachegrind
    valgrind --tool=cachegrind --cachegrind-out-file=$cachegrind_out ./sobel_par input1.jpg $threads

    # Generate annotated report
    cg_annotate $cachegrind_out > $report_file
done

echo "Cachegrind analysis completed."
