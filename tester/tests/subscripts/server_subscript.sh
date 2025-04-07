#!/bin/bash

# Check if an argument is provided
if [ $# -ne 5 ]; then
    echo "Usage: $0 <root_dir> <port_num> <threads> <buffers> <sched_alg>"
    exit 1
fi

timeout 6 ./tests-bin/pserver -d $1 -p $2 -t $3 -b $4 -s $5
rc=$?

if [ $rc -ne 124 ]; then
    echo "Unexpected return code: $rc" >&2
    exit 1
fi
