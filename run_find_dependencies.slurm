#!/bin/bash
#SBATCH --job-name=find_deps_single
#SBATCH --output=find_deps_%j.out
#SBATCH --error=find_deps_%j.err
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=2G
#SBATCH --time=00:10:00

# Load necessary modules if required (system dependent)
# module load cmake gcc boost

# Path to the executable
CLI_TOOL="./find_dependencies"
if [ ! -f "$CLI_TOOL" ]; then
    echo "Error: $CLI_TOOL not found. Please compile it first."
    exit 1
fi

# Path to a benchmark file
BENCHMARK_FILE="./scripts/benchmarks-ltl/01.txt"

if [ ! -f "$BENCHMARK_FILE" ]; then
    echo "Error: Benchmark file $BENCHMARK_FILE not found."
    exit 1
fi

# Extract fields from the benchmark file (robustly handling "Key: Value" format)
# We use grep and cut to handle the labelled format found in scripts/benchmarks-ltl
BENCHMARK_NAME=$(grep "^Name:" "$BENCHMARK_FILE" | cut -d: -f2 | xargs)
FORMULA=$(grep "^Formula:" "$BENCHMARK_FILE" | cut -d: -f2- | xargs)
INPUTS=$(grep "^Input:" "$BENCHMARK_FILE" | cut -d: -f2 | xargs)
OUTPUTS=$(grep "^Output:" "$BENCHMARK_FILE" | cut -d: -f2 | xargs)

# Fallback if name is missing
if [ -z "$BENCHMARK_NAME" ]; then
    BENCHMARK_NAME=$(basename "$BENCHMARK_FILE" .txt)
fi

echo "Running benchmark: $BENCHMARK_NAME"

# specific algorithm: formula or automaton
ALGO="automaton"

CMD="$CLI_TOOL --formula=\"$FORMULA\" --model-name=\"$BENCHMARK_NAME\" --algo=$ALGO"

if [ -n "$INPUTS" ]; then
    CMD="$CMD --input=\"$INPUTS\""
fi
if [ -n "$OUTPUTS" ]; then
    CMD="$CMD --output=\"$OUTPUTS\""
fi

echo "Executing: $CMD"

# Run with timeout
time timeout 10m bash -c "$CMD"
