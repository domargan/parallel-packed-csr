#!/bin/bash

# Written by Christian Menges, 10. July 2020

time=$(date +"%y_%m_%d_%k:%M:%S")
outFile="${time}_strong_scaling_results.csv"

repetitions=3

rm -f $outFile

header="threads"
for ((r = 0; r < repetitions; r++)); do
  header="${header},insertiontime${r}"
done
header="${header},insertiontimeAvg"

for ((r = 0; r < repetitions; r++)); do
  header="${header},deletiontime${r}"
done
header="${header},deletiontimeAvg"

echo "$header" | tee $outFile

for i in {0..6}; do
  t=$((2 ** i))
  insert=""
  for ((r = 0; r < repetitions; r++)); do
    output=$(./parallel-packed-csr -threads=$t -update_file=../data/update_files/insertions.txt | sed '/Elapsed/!d' | sed -n '0~2p' | sed 's/Elapsed wall clock time: //g')
    insert="${insert},${output}"
  done
  avg=$(echo "$insert" | awk '{l=split($0,a,","); s=0; for (i in a)s+=a[i]; print s/(l-1);}')
  insert="${insert},${avg}"

  delete=""
  for ((r = 0; r < repetitions; r++)); do
    output=$(./parallel-packed-csr -delete -threads=$t -update_file=../data/update_files/deletions.txt | sed '/Elapsed/!d' | sed -n '0~2p' | sed 's/Elapsed wall clock time: //g')
    delete="${delete},${output}"
  done
  avg=$(echo "$delete" | awk '{l=split($0,a,","); s=0; for (i in a)s+=a[i]; print s/(l-1);}')
  delete="${delete},${avg}"

  line="${insert}${delete}"

  echo "$line" | sed -e "s/^/$t/" | tee -a $outFile
done
