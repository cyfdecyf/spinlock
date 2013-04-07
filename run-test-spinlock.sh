#!/bin/bash

function run_test() {
    for nthr in 1 2 4 8 16 32; do
        ./$1 $nthr > /dev/null
        for i in `seq 1 3`; do
            ./$1 $nthr
        done
        echo
    done
}

echo "test spin lock using cmpxchg"
run_test "test-spinlock-cmpxchg"

echo "test spin lock using xchg"
run_test "test-spinlock-xchg"

echo "test spin lock using k42"
run_test "test-spinlock-k42"

echo "test spin lock using mcs"
run_test "test-spinlock-mcs"

echo "test spin lock using ticket"
run_test "test-spinlock-ticket"

echo "test spin lock using pthread"
run_test "test-spinlock-pthread"

echo "test spin lock using xchg-backoff"
run_test "test-spinlock-xchg-backoff"

