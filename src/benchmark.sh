#!/bin/bash
# Written by Christian Menges and Domagoj Margan, August 2020

######################################

# Define project paths
PPCSR_PATH="$HOME/parallel-packed-csr"
PPCSR_EXEC="$PPCSR_PATH/build/parallel-packed-csr"

# Define input files
PPCSR_INSERTIONS_FILE="$PPCSR_PATH/data/update_files/insertions.txt"
PPCSR_DELETIONS_FILE="$PPCSR_PATH/data/update_files/deletions.txt"

# Define output files
TIME=$(date +%Y%m%d_%H%M%S)
PPCSR_LOG="strong_scaling_measurements_$TIME.csv"
PPCSR_PLOT_DATA="strong_scaling_measurements_$TIME.dat"
PPCSR_PLOT_FILE=$(mktemp gnuplot.pXXX)
PPCSR_PDF_PLOT_FILE=ppcsr_strong_scaling_plot

# Define experiment parameters
REPETITIONS=3
CORES=(1 5 10 15 20)


######################################

# Write headers to CSV log
header="#CORES"
for ((r = 0; r < REPETITIONS; r++)); do
  header="${header},INS${r}"
done
header="${header},INS_Avg"

for ((r = 0; r < REPETITIONS; r++)); do
  header="${header},DEL${r}"
done
header="${header},DEL_Avg"

echo "$header" | tee $PPCSR_LOG

# Run the scaling experiment and write measures to the CSV log
for core in ${CORES[@]}; do
  insert=""
  for ((r = 0; r < REPETITIONS; r++)); do
    output=$($PPCSR_EXEC -threads=$core -update_file=$PPCSR_INSERTIONS_FILE | sed '/Elapsed/!d' | sed -n '0~2p' | sed 's/Elapsed wall clock time: //g')
    insert="${insert},${output}"
  done
  avg_insert=$(echo "$insert" | awk '{l=split($0,a,","); s=0; for (i in a)s+=a[i]; print s/(l-1);}')
  insert="${insert},${avg_insert}"

  delete=""
  for ((r = 0; r < REPETITIONS; r++)); do
    output=$($PPCSR_EXEC -delete -threads=$core -update_file=$PPCSR_DELETIONS_FILE | sed '/Elapsed/!d' | sed -n '0~2p' | sed 's/Elapsed wall clock time: //g')
    delete="${delete},${output}"
  done
  avg_delete=$(echo "$delete" | awk '{l=split($0,a,","); s=0; for (i in a)s+=a[i]; print s/(l-1);}')
  delete="${delete},${avg_delete}"

  line="${insert}${delete}"

  echo "$line" | sed -e "s/^/$core/" | tee -a $PPCSR_LOG
  echo $core $avg_insert $avg_delete >>$PPCSR_PLOT_DATA
done


######################################

# Create the plot
XLABEL="#cores"
YLABEL="CPU time (ms)"

cat <<EOF >$PPCSR_PLOT_FILE
set term pdf monochrome font ", 14"
set output "${PPCSR_PDF_PLOT_FILE}.pdf"
set xlabel "${XLABEL}"
set ylabel "${YLABEL}" offset 1.5
set size ratio 0.5
#set size 0.8,0.8
EOF

echo -n 'set xtics (' >>$PPCSR_PLOT_FILE
for i in ${CORES[@]}; do
  echo -n " $i," >>$PPCSR_PLOT_FILE
done
echo ')' >>$PPCSR_PLOT_FILE

cat <<EOF >>$PPCSR_PLOT_FILE
#set ytics nomirror
set key left top
#set key font ",12"
set xrange [${CORES[0]}:${CORES[-1]}]
set yrange [0:]
plot \
	"$PPCSR_PLOT_DATA" using 1:2 title 'insertions' with linespoint, \
	"$PPCSR_PLOT_DATA" using 1:3 title 'deletions' with linespoint
EOF

gnuplot $PPCSR_PLOT_FILE
rm $PPCSR_PLOT_FILE
pdfcrop --margins "0 0 0 0" --clip ${PPCSR_PDF_PLOT_FILE}.pdf ${PPCSR_PDF_PLOT_FILE}.pdf &>/dev/null
