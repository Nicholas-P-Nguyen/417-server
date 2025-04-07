#!/bin/bash

if [ $# -ne 4 ]; then
    echo "Usage: $0 <num_clients> <num_threads> <num_buffers> <filename>"
    exit 1
fi

port_num=$(( RANDOM % 18001 + 2000 ))

./tests/subscripts/server_subscript.sh ./tests/pages "$port_num" "$2" "$3" FIFO &

sleep 1s

for (( i=0; i<$1; i++ )); do
    ./tests/subscripts/client_subscript.sh localhost "$port_num" "$4" &
done

wait