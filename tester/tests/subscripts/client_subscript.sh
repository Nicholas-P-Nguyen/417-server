#!/bin/bash

# Check if an argument is provided
if [ $# -ne 3 ]; then
    echo "Usage: $0 <host> <port_num> <filename>"
    exit 1
fi

timeout 5 ./tests-bin/pclient $1 $2 $3
rc=$?

if [ $rc -ne 0 ]; then
    echo "Unexpected return code: $rc" >&2
    exit 1
fi
