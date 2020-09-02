#!/bin/bash
# Written by Christian Menges and Domagoj Margan, August 2020

######################################

# Pass the benchmark config file path as a first script argument
BENCHMARK_CONFIG_FILE="$1"

# The config file should contain and define the following variables:

# Machine and dataset info for plotting:
# MACHINE_NAME              -> Name of the testbed machine
# DATASET_NAME              -> Dataset alias

# Program and data input file paths:
# PPCSR_EXEC                -> program binary file
# PPCSR_CORE_GRAPH_FILE     -> core graph edgelist file
# PPCSR_INSERTIONS_FILE     -> insertions update file
# PPCSR_DELETIONS_FILE      -> deletions update file

# Experiment parameters:
# REPETITIONS               -> number of times to repeat the benchmark; integer
# CORES                     -> number of cores to utilise in the scaling benchmark; array of integers
# NUMA_BOUNDS               -> number of cores that mark end of NUMA domain boundaries; array of integers
# PARTITIONS_PER_DOMAIN     -> number of partitions per NUMA domain; array of integers
# SIZE                      -> number of edges that will be read from the update file; integer

source $BENCHMARK_CONFIG_FILE
if [ ! -f "$PPCSR_EXEC" ] ||
	 [ ! -f "$PPCSR_CORE_GRAPH_FILE" ] ||
	 [ ! -f "$PPCSR_INSERTIONS_FILE" ] ||
	 [ ! -f "$PPCSR_DELETIONS_FILE" ]; then
  echo -e "All required input data files are not found.\n"
  exit 0
fi

# Define output files
TIME=$(date +%Y%m%d_%H%M%S)
PPCSR_BENCHMARK_LOG="${MACHINE_NAME}_strong_scaling_log_${TIME}.txt"
PPCSR_CSV_DATA="${MACHINE_NAME}_strong_scaling_${TIME}.csv"
PPCSR_PLOT_DATA="${MACHINE_NAME}_strong_scaling_${TIME}.dat"
PPCSR_PDF_PLOT_FILE="${MACHINE_NAME}_strong_scaling_${TIME}"

# Write everyting to log file
: > $PPCSR_BENCHMARK_LOG
exec 2> >(tee -a $PPCSR_BENCHMARK_LOG >&2) > >(tee -a $PPCSR_BENCHMARK_LOG)


######################################

echo "######################################"
echo "Starting benchmark: strong scaling"

echo "Testbed machine: $MACHINE_NAME"
echo "Dataset: $DATASET_NAME"
echo "Core graph file: $PPCSR_CORE_GRAPH_FILE"
echo "Edge insertions file: $PPCSR_INSERTIONS_FILE"
echo "Edge deletions file: $PPCSR_DELETIONS_FILE"
echo "Repetitions: $REPETITIONS"
echo "#cores: ${CORES[*]}"
echo "NUMA domain boundaries: ${NUMA_BOUNDS[*]}"
echo "#partitions per NUMA domain: ${PARTITIONS_PER_DOMAIN[*]}"
echo "Update batch size: $SIZE"
echo -e "######################################\n"

######################################

echo -e "[START]\t Starting computations...\n"

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

echo "$header" >>$PPCSR_CSV_DATA

# Run the scaling experiment and write measures to the CSV log
for core in ${CORES[@]}; do
  csv=""
  dat=""
  for v in -ppcsr -pppcsr -pppcsrnuma; do
    for p in ${PARTITIONS_PER_DOMAIN[@]}; do
      if [ "$v" = "-ppcsr" ]; then
        p=1
      fi
      insert=""
      for ((r = 1; r <= REPETITIONS; r++)); do
        echo -e "[START]\t ${v:1} edge insertions: Executing repetition #$r on $core cores..."
        output=$($PPCSR_EXEC -threads=$core $v -size=$SIZE -core_graph=$PPCSR_CORE_GRAPH_FILE -update_file=$PPCSR_INSERTIONS_FILE -partitions_per_domain=$p | sed '/Elapsed/!d' | sed -n '0~2p' | sed 's/Elapsed wall clock time: //g')
        echo -e "[END]  \t ${v:1} edge insertions: Finished repetition #$r on $core cores.\n"
        insert="${insert},${output}"
      done
      avg_insert=$(echo "$insert" | awk '{l=split($0,a,","); s=0; for (i in a)s+=a[i]; print s/(l-1);}')
      insert="${insert},${avg_insert}"

      delete=""
      for ((r = 1; r <= REPETITIONS; r++)); do
        echo -e "[START]\t ${v:1} edge deletions: Executing repetition #$r on $core cores..."
        output=$($PPCSR_EXEC -delete -threads=$core $v -size=$SIZE -core_graph=$PPCSR_CORE_GRAPH_FILE -update_file=$PPCSR_DELETIONS_FILE -partitions_per_domain=$p | sed '/Elapsed/!d' | sed -n '0~2p' | sed 's/Elapsed wall clock time: //g')
        echo -e "[END]  \t ${v:1} edge deletions: Finished repetition #$r on $core cores.\n"
        delete="${delete},${output}"
      done
      avg_delete=$(echo "$delete" | awk '{l=split($0,a,","); s=0; for (i in a)s+=a[i]; print s/(l-1);}')
      delete="${delete},${avg_delete}"

      csv="${csv}${insert}${delete}"
      dat="${dat}${avg_insert} ${avg_delete} "

      if [ "$v" = "-ppcsr" ]; then
        break
      fi
    done
  done

  echo "$csv" | sed -e "s/^/$core/" >>$PPCSR_CSV_DATA
  echo $core $dat >>$PPCSR_PLOT_DATA
done

echo -e "[END]  \t Computations finished.\n"

######################################

# Create the plot

echo -e "[START]\t Starting data plotting...\n"

PPCSR_PLOT_FILE=$(mktemp gnuplot.pXXX)

XLABEL="#cores"
YLABEL="CPU time (ms)"

cat <<EOF >$PPCSR_PLOT_FILE
set term pdf font ", 12"
set output "${PPCSR_PDF_PLOT_FILE}.pdf"
set title "Machine: $MACHINE_NAME \t Dataset: $DATASET_NAME \t #Updates: $SIZE"
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
set key right top
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

echo -e "[END]  \t Plotting finished.\n"

echo "Exiting benchmark."

exit 0
