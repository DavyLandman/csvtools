#!/bin/bash

INPUT_FILE=$1
COLUMNS=$(head -n 1 "$INPUT_FILE")
PROGRAMS="csvcut csvgrep csvawk"

echo "csvkit, cut, sed, grep, awk, csvtools"

for $program in PROGRAMS
do
    BENCH_COLUMN=


done


