#!/bin/bash

#SBATCH --job-name=find_input_deps
#SBATCH --output={{OUTPUT_BASE_PATH}}/%a.out
#SBATCH --error={{OUTPUT_BASE_PATH}}/%a.err
#SBATCH --array=1-{{NUM_BENCHMARKS}}
#SBATCH --ntasks=1
#SBATCH --mem=8G
#SBATCh --cpus-per-task=1

TOTAL_TIMEOUT="{{TIMEOUT}}"
CLI_TOOL="./find_input_dependencies"
BENCHMARKS_DIR="{{BENCHMARKS_DIR}}"
DEPENDENCY_TIMEOUT="{{FIND_DEP_TIMEOUT}}"
SEARCH_PATH="$(pwd)/hpc_deps"

# Setup environment
source "$(pwd)/scripts/hpc_setup.sh"

# Create an array of all txt files in the benchmarks directory
FILES=($(ls $BENCHMARKS_DIR/*.txt | sort))
# Get the file corresponding to the current array task ID
FILEPATH=${FILES[$SLURM_ARRAY_TASK_ID-1]}

# Extract formulas and vars using labels or position
# Supports both:
# Label: Value
# and positional (lines 2-8)
if grep -q ":" "$FILEPATH"; then
    benchmark_name=$(grep -i "^Name:" "$FILEPATH" | cut -d':' -f2- | xargs)
    inputs_var=$(grep -i "^Input:" "$FILEPATH" | cut -d':' -f2- | xargs)
    outputs_var=$(grep -i "^Output:" "$FILEPATH" | cut -d':' -f2- | xargs)
    formula=$(grep -i "^Formula:" "$FILEPATH" | cut -d':' -f2- | xargs)
else
    # Positional fallback
    benchmark_name=$(sed -n "2p" "$FILEPATH" | tr -d '\r')
    formula=$(sed -n "4p" "$FILEPATH" | tr -d '\r')
    inputs_var=$(sed -n "7p" "$FILEPATH" | tr -d '\r')
    outputs_var=$(sed -n "8p" "$FILEPATH" | tr -d '\r')
fi

if [[ -z "$formula" ]]; then
    echo "Warning: Formula is empty for $benchmark_name ($FILEPATH), skipping."
    exit 0
fi

cmd_string="$CLI_TOOL --formula=\"$formula\" --model-name=\"$benchmark_name\" --dependency-timeout=$DEPENDENCY_TIMEOUT --verbose --algo={{ALGORITHM}}"
if [[ -n "$inputs_var" ]]; then
    cmd_string="$cmd_string --input=\"$inputs_var\""
fi
if [[ -n "$outputs_var" ]]; then
    cmd_string="$cmd_string --output=\"$outputs_var\""
fi

echo "Running: $cmd_string"
srun bash -c "{ time timeout $TOTAL_TIMEOUT $cmd_string; }"
