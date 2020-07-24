#!/bin/bash

# Written by Christian Menges, 10. July 2020

outFile=strong_scaling_results.csv

repetitions=3

rm -f $outFile

header="threads"
for ((r = 0; r < repetitions; r++)); do
  header="${header},instime${r}"
done

for ((r = 0; r < repetitions; r++)); do
  header="${header},deltime${r}"
done
echo "$header" | tee $outFile

for i in {0..6}; do
  t=$((2 ** i))
  line=""
  for ((r = 0; r < repetitions; r++)); do
    output=$(./parallel-packed-csr -threads=$t -update_file=../data/update_files/insertions.txt | sed '/Elapsed/!d' | sed -n '0~2p' | sed 's/Elapsed wall clock time: //g')
    line="${line},${output}"
  done

  for ((r = 0; r < repetitions; r++)); do
    output=$(./parallel-packed-csr -delete -threads=$t -update_file=../data/update_files/deletions.txt | sed '/Elapsed/!d' | sed -n '0~2p' | sed 's/Elapsed wall clock time: //g')
    line="${line},${output}"
  done

  echo "$line" | sed -e "s/^/$t/" | tee -a $outFile
done
