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
# CORES                     -> number of cores to utilise in the benchmark; integer
# PARTITIONS_PER_DOMAIN     -> number of partitions per NUMA domain; array of integers
# SIZE                      -> number of edges that will be read from the update file; integer

source $BENCHMARK_CONFIG_FILE
if [ ! -f "$PPCSR_EXEC" ]; then
  echo -e "Executable not found.\n"
  exit 0
fi

if [ ! -f "$PPCSR_CORE_GRAPH_FILE" ]; then
  echo -e "Core graph not found.\n"
  exit 0
fi

if [ ! -f "$PPCSR_INSERTIONS_FILE" ] ||
	 [ ! -f "$PPCSR_DELETIONS_FILE" ]; then
  echo -e "Update files not found.\n"
  exit 0
fi

# Define output files
TIME=$(date +%Y%m%d_%H%M%S)
PPCSR_BASE_NAME="${MACHINE_NAME}_${TIME}_ppcsr_partitioning"
PPCSR_BENCHMARK_OUTPUTS_DIR="${PPCSR_BASE_NAME}_bench_outputs"
PPCSR_PROGRAM_OUTPUTS_DIR="${PPCSR_BENCHMARK_OUTPUTS_DIR}/program_outputs"
PPCSR_BENCHMARK_LOG="${PPCSR_BENCHMARK_OUTPUTS_DIR}/${PPCSR_BASE_NAME}_script_log.txt"
PPCSR_CSV_DATA="${PPCSR_BENCHMARK_OUTPUTS_DIR}/${PPCSR_BASE_NAME}_all_results.csv"
PPCSR_PLOT_DATA="${PPCSR_BENCHMARK_OUTPUTS_DIR}/${PPCSR_BASE_NAME}_plot_data.dat"
PPCSR_PDF_PLOT_FILE="${PPCSR_BENCHMARK_OUTPUTS_DIR}/${PPCSR_BASE_NAME}_plot"

mkdir $PPCSR_BENCHMARK_OUTPUTS_DIR $PPCSR_PROGRAM_OUTPUTS_DIR

# Write everyting to log file
: > $PPCSR_BENCHMARK_LOG
exec 2> >(tee -a $PPCSR_BENCHMARK_LOG >&2) > >(tee -a $PPCSR_BENCHMARK_LOG)

######################################

echo "######################################"
echo "Starting benchmark: partitioning"

echo "Testbed machine: $MACHINE_NAME"
echo "Dataset: $DATASET_NAME"
echo "Core graph file: $PPCSR_CORE_GRAPH_FILE"
echo "Edge insertions file: $PPCSR_INSERTIONS_FILE"
echo "Edge deletions file: $PPCSR_DELETIONS_FILE"
echo "Repetitions: $REPETITIONS"
echo "#cores: ${CORES}"
echo "#partitions per NUMA domain: ${PARTITIONS_PER_DOMAIN[*]}"
echo "Update batch size: $SIZE"
echo -e "######################################\n"

######################################

echo -e "[START]\t Starting computations...\n"

# Write headers to CSV log
header="#PARTITIONS"
function writeHeader() {
  for ((r = 0; r < REPETITIONS; r++)); do
    header="${header} $1${r}"
  done
  header="${header} $1_Avg $1_Stddev"
}

writeHeader "INS_PPPCSR"
writeHeader "DEL_PPPCSR"
writeHeader "INS_PPPCSR_NUMA"
writeHeader "DEL_PPPCSR_NUMA"

echo "$header" >>$PPCSR_CSV_DATA
echo "partitions ins del ins-NUMA del-NUMA" >>$PPCSR_PLOT_DATA

# Run the partitioning and write measures to the CSV log
for p in ${PARTITIONS_PER_DOMAIN[@]}; do
  csv=""
  dat=""
  for v in -pppcsr -pppcsrnuma; do
    insert=""
    for ((r = 1; r <= REPETITIONS; r++)); do
      echo -e "[START]\t ${v:1} edge insertions: Executing repetition #$r on $CORES cores for $p partitions per NUMA domain..."
      output=$($PPCSR_EXEC -threads=$CORES $v -size=$SIZE -core_graph=$PPCSR_CORE_GRAPH_FILE -update_file=$PPCSR_INSERTIONS_FILE -partitions_per_domain=$p 2>&1 | tee "${PPCSR_PROGRAM_OUTPUTS_DIR}/${PPCSR_BASE_NAME}_insertions_${v:1}_${CORES}cores_${p}par_${r}.txt" | sed '/Elapsed/!d' | sed -n '0~2p' | sed 's/Elapsed wall clock time: //g')
      echo -e "[END]  \t ${v:1} edge insertions: Finished repetition #$r on $CORES cores for $p partitions per NUMA domain.\n"
      insert="${insert} ${output}"
    done

    if [ "$REPETITIONS" -gt 1 ]; then
      read avg_insert stddev_insert <<<$(echo "$insert" | awk '{ A=0; V=0; for(N=1; N<=NF; N++) A+=$N ; A/=NF ; for(N=1; N<=NF; N++) V+=(($N-A)*($N-A))/(NF-1); print A,sqrt(V) }')
    else
      avg_insert=$insert
      stddev_insert=0
    fi
    insert="${insert} ${avg_insert} ${stddev_insert}"

    delete=""
    for ((r = 1; r <= REPETITIONS; r++)); do
      echo -e "[START]\t ${v:1} edge deletions: Executing repetition #$r on $CORES cores for $p partitions per NUMA domain..."
      output=$($PPCSR_EXEC -delete -threads=$CORES $v -size=$SIZE -core_graph=$PPCSR_CORE_GRAPH_FILE -update_file=$PPCSR_DELETIONS_FILE -partitions_per_domain=$p 2>&1 | tee "${PPCSR_PROGRAM_OUTPUTS_DIR}/${PPCSR_BASE_NAME}_deletions_${v:1}_${CORES}cores_${p}par_${r}.txt" | sed '/Elapsed/!d' | sed -n '0~2p' | sed 's/Elapsed wall clock time: //g')
      echo -e "[END]  \t ${v:1} edge deletions: Finished repetition #$r on $CORES cores for $p partitions per NUMA domain.\n"
      delete="${delete} ${output}"
    done

    if [ "$REPETITIONS" -gt 1 ]; then
      read avg_delete stddev_delete <<<$(echo "$delete" | awk '{ A=0; V=0; for(N=1; N<=NF; N++) A+=$N ; A/=NF ; for(N=1; N<=NF; N++) V+=(($N-A)*($N-A))/(NF-1); print A,sqrt(V) }')
    else
      avg_delete=$delete
      stddev_delete=0
    fi
    delete="${delete} ${avg_delete} ${stddev_delete}"

    csv="${csv}${insert} ${delete}"
    dat="${dat}${avg_insert} ${avg_delete} "
  done

  echo "$csv" | sed -e "s/^/$p/" >>$PPCSR_CSV_DATA
  echo $p $dat >>$PPCSR_PLOT_DATA
done

echo -e "[END]  \t Computations finished.\n"

######################################

# Create the plot

echo -e "[START]\t Starting data plotting...\n"

PPCSR_PLOT_FILE=$(mktemp gnuplot.pXXX)
PPCSR_PLOT_DATA_TRANSP=$(mktemp gnuplot.datXXX)

awk '
{
    for (i=1; i<=NF; i++)  {
        a[NR,i] = $i
    }
}
NF>p { p = NF }
END {
    for(j=1; j<=p; j++) {
        str=a[1,j]
        for(i=2; i<=NR; i++){
            str=str" "a[i,j];
        }
        print str
    }

}' $PPCSR_PLOT_DATA >$PPCSR_PLOT_DATA_TRANSP

XLABEL="#Partitions per NUMA domain"
YLABEL="CPU Time (ms)"

cat <<EOF >$PPCSR_PLOT_FILE
set term pdf font ", 12"
set output "${PPCSR_PDF_PLOT_FILE}.pdf"

set title font ", 10"
set title "Machine: $MACHINE_NAME \t Threads: $CORES \t Dataset: $DATASET_NAME \t #Updates: $SIZE"
set xlabel "${XLABEL}"
set ylabel "${YLABEL}" offset 1.5
set size ratio 0.5

set key right top
set key font ", 10"

set style data histograms
set style histogram cluster gap 1
set style fill solid 0.3
set boxwidth 0.9
set auto x
set xtic scale 0
set yrange [0:]

N = system("awk 'NR==1{print NF}' $PPCSR_PLOT_DATA_TRANSP")

plot for [COL=2:N] "$PPCSR_PLOT_DATA_TRANSP" using COL:xtic(1) title columnheader
EOF

gnuplot $PPCSR_PLOT_FILE
rm $PPCSR_PLOT_FILE
rm $PPCSR_PLOT_DATA_TRANSP
pdfcrop --margins "0 0 0 0" --clip ${PPCSR_PDF_PLOT_FILE}.pdf ${PPCSR_PDF_PLOT_FILE}.pdf &>/dev/null

echo -e "[END]  \t Plotting finished.\n"

echo "Exiting benchmark."

exit 0

