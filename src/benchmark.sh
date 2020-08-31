#!/bin/bash
# Written by Christian Menges and Domagoj Margan, August 2020

######################################

# Define project paths
PPCSR_PATH="$HOME/parallel-packed-csr"
PPCSR_EXEC="$PPCSR_PATH/build/parallel-packed-csr"

# Define input files
PPCSR_CORE_GRAPH_FILE="$1"
PPCSR_INSERTIONS_FILE="$2"
PPCSR_DELETIONS_FILE="$3"

# Define output files
TIME=$(date +%Y%m%d_%H%M%S)
PPCSR_LOG="strong_scaling_measurements_$TIME.csv"
PPCSR_PLOT_DATA="strong_scaling_measurements_$TIME.dat"
PPCSR_PLOT_FILE=$(mktemp gnuplot.pXXX)
PPCSR_PDF_PLOT_FILE=ppcsr_strong_scaling_plot

# Define experiment parameters
REPETITIONS=2
CORES=(1 5 10 15 20)
NUMA_BOUNDS=(10)

SIZE=1000000

######################################

echo "Core graph file: $PPCSR_CORE_GRAPH_FILE"
echo "Edge insertions file: $PPCSR_INSERTIONS_FILE"
echo "Edge deletions file: $PPCSR_DELETIONS_FILE"

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
      output=$($PPCSR_EXEC -threads=$core $v -size=$SIZE -core_graph=$PPCSR_CORE_GRAPH_FILE -update_file=$PPCSR_INSERTIONS_FILE | sed '/Elapsed/!d' | sed -n '0~2p' | sed 's/Elapsed wall clock time: //g')
      insert="${insert},${output}"
    done
    avg_insert=$(echo "$insert" | awk '{l=split($0,a,","); s=0; for (i in a)s+=a[i]; print s/(l-1);}')
    insert="${insert},${avg_insert}"

    delete=""
    for ((r = 0; r < REPETITIONS; r++)); do
      output=$($PPCSR_EXEC -delete -threads=$core $v -size=$SIZE -core_graph=$PPCSR_CORE_GRAPH_FILE -update_file=$PPCSR_DELETIONS_FILE | sed '/Elapsed/!d' | sed -n '0~2p' | sed 's/Elapsed wall clock time: //g')
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
set term pdf font ", 12"
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

for i in ${NUMA_BOUNDS[@]}; do
  echo "set arrow from $i, graph 0 to $i, graph 1 nohead dt 3" >>$PPCSR_PLOT_FILE
done

cat <<EOF >>$PPCSR_PLOT_FILE
#set ytics nomirror
set key left top
set key font ",12"

set style line 1 lt 1 lc rgb "blue" lw 1 pt 5 ps 0.5
set style line 2 lt 1 dt 4 lc rgb "blue" lw 1 pt 4 ps 0.5
set style line 3 lt 1 lc rgb "red" lw 1 pt 7 ps 0.5
set style line 4 lt 1 dt 4 lc rgb "red" lw 1 pt 6 ps 0.5
set style line 5 lt 1 lc rgb "green" lw 1 pt 9 ps 0.5
set style line 6 lt 1 dt 4 lc rgb "green" lw 1 pt 8 ps 0.5

set xrange [${CORES[0]}:${CORES[-1]}]
set yrange [0:]
plot \
	"$PPCSR_PLOT_DATA" using 1:2 title 'insertions' with linespoint ls 1, \
	"$PPCSR_PLOT_DATA" using 1:3 title 'deletions' with linespoint ls 2, \
	"$PPCSR_PLOT_DATA" using 1:4 title 'insertions par' with linespoint ls 3, \
	"$PPCSR_PLOT_DATA" using 1:5 title 'deletions par' with linespoint ls 4, \
	"$PPCSR_PLOT_DATA" using 1:6 title 'insertions numa' with linespoint ls 5, \
	"$PPCSR_PLOT_DATA" using 1:7 title 'deletions numa' with linespoint ls 6
EOF

gnuplot $PPCSR_PLOT_FILE
rm $PPCSR_PLOT_FILE
pdfcrop --margins "0 0 0 0" --clip ${PPCSR_PDF_PLOT_FILE}.pdf ${PPCSR_PDF_PLOT_FILE}.pdf &>/dev/null
