#!/usr/bin/env bash
set -euo pipefail

taskforge_executable="${1:-./build-release/taskforge}"
taskforge_tasks="${TASKFORGE_BENCHMARK_TASKS:-100000}"
taskforge_queue_capacity="${TASKFORGE_BENCHMARK_QUEUE_CAPACITY:-256}"

for taskforge_workers in 1 2 4 8; do
    for taskforge_run in 1 2 3; do
        printf 'run=%s workers=%s\n' "${taskforge_run}" "${taskforge_workers}"
        "${taskforge_executable}" benchmark \
            --workers "${taskforge_workers}" \
            --tasks "${taskforge_tasks}" \
            --queue-capacity "${taskforge_queue_capacity}"
    done
done
