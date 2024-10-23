#!/bin/bash

# Array of thread counts
thread_counts=(2 4 8 16 32)

# Loop over each thread count
for threads in "${thread_counts[@]}"; do
    echo "Running Threading analysis with $threads threads..."

    # Set result directory name
    result_dir="vtune_threading_par_${threads}"

    # Run VTune Hotspots analysis with concurrency profiling
    vtune -collect hotspots -knob sampling-mode=sw -result-dir $result_dir -- ./sobel_par input1.jpg $threads
done

echo "Threading analysis completed."
