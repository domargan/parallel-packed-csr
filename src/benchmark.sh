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

SIZE=1000000

######################################

# Write headers to CSV log
header="#CORES"
function writeHeader() {
  for ((r = 0; r < REPETITIONS; r++)); do
    header="${header},$1${r}"
  done
  header="${header},$1_Avg"
}
writeHeader "INS_PPCSR"
writeHeader "DEL_PPCSR"
writeHeader "INS_PPPCSR"
writeHeader "DEL_PPPCSR"
writeHeader "INS_PPPCSR_NUMA"
writeHeader "DEL_PPPCSR_NUMA"

echo "$header" | tee $PPCSR_LOG

# Run the scaling experiment and write measures to the CSV log
for core in ${CORES[@]}; do
  line=""
  dat=""
  for v in -ppcsr -pppcsr -pppcsrnuma; do
    insert=""
    for ((r = 0; r < REPETITIONS; r++)); do
      output=$($PPCSR_EXEC -threads=$core $v -size=$SIZE -update_file=$PPCSR_INSERTIONS_FILE | sed '/Elapsed/!d' | sed -n '0~2p' | sed 's/Elapsed wall clock time: //g')
      insert="${insert},${output}"
    done
    avg_insert=$(echo "$insert" | awk '{l=split($0,a,","); s=0; for (i in a)s+=a[i]; print s/(l-1);}')
    insert="${insert},${avg_insert}"

    delete=""
    for ((r = 0; r < REPETITIONS; r++)); do
      output=$($PPCSR_EXEC -delete -threads=$core $v -size=$SIZE -update_file=$PPCSR_DELETIONS_FILE | sed '/Elapsed/!d' | sed -n '0~2p' | sed 's/Elapsed wall clock time: //g')
      delete="${delete},${output}"
    done
    avg_delete=$(echo "$delete" | awk '{l=split($0,a,","); s=0; for (i in a)s+=a[i]; print s/(l-1);}')
    delete="${delete},${avg_delete}"

    line="${line}${insert}${delete}"
    dat="${dat}${avg_insert} ${avg_delete} "

  done

  echo "$line" | sed -e "s/^/$core/" | tee -a $PPCSR_LOG
  echo $core $dat >>$PPCSR_PLOT_DATA
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
	"$PPCSR_PLOT_DATA" using 1:3 title 'deletions' with linespoint, \
	"$PPCSR_PLOT_DATA" using 1:4 title 'insertions par' with linespoint, \
	"$PPCSR_PLOT_DATA" using 1:5 title 'deletions par' with linespoint, \
	"$PPCSR_PLOT_DATA" using 1:6 title 'insertions numa' with linespoint, \
	"$PPCSR_PLOT_DATA" using 1:7 title 'deletions numa' with linespoint
EOF

gnuplot $PPCSR_PLOT_FILE
rm $PPCSR_PLOT_FILE
pdfcrop --margins "0 0 0 0" --clip ${PPCSR_PDF_PLOT_FILE}.pdf ${PPCSR_PDF_PLOT_FILE}.pdf &>/dev/null
