#!/bin/bash

#SBATCH --job-name=inp_dep_pddl
#SBATCH --output={{OUTPUT_BASE_PATH}}/%a.out
#SBATCH --error={{OUTPUT_BASE_PATH}}/%a.err
#SBATCH --array=1-{{NUM_BENCHMARKS}}
#SBATCH --ntasks=1
#SBATCH --mem=4G
#SBATCH --cpus-per-task=1

TOTAL_TIMEOUT="{{TIMEOUT}}"
CLI_TOOL="./inp_dep_synthesis"
FILEPATH="{{BENCHMARKS_DIR}}/$SLURM_ARRAY_TASK_ID.txt"
ALGORITHM="{{ALGORITHM}}"

# Setup environment
source "$(pwd)/scripts/hpc_setup.sh"

if [ ! -f "$FILEPATH" ]; then
    echo "File $FILEPATH not found, skipping."
    exit 0
fi

benchmark_family=$(sed -n "3p" "$FILEPATH" | tr -d '\r')
benchmark_name=$(sed -n "2p" "$FILEPATH" | tr -d '\r')
formula=$(sed -n "4p" "$FILEPATH" | tr -d '\r')
env_formula=$(sed -n "5p" "$FILEPATH" | tr -d '\r')
sys_formula=$(sed -n "6p" "$FILEPATH" | tr -d '\r')
inputs_var=$(sed -n "7p" "$FILEPATH" | tr -d '\r')
outputs_var=$(sed -n "8p" "$FILEPATH" | tr -d '\r')

allowed_family=({{ALLOWED_FAMILIES}})
if [[ ! " ${allowed_family[@]} " =~ " ${benchmark_family} " ]]; then
    echo "Family $benchmark_family is skipped"
    exit 1
fi

cmd_string="$CLI_TOOL -e \"$env_formula\" -s \"$sys_formula\" --input \"$inputs_var\" --output \"$outputs_var\" --algorithm $ALGORITHM"

echo "Running: $cmd_string"
srun bash -c "{ time timeout $TOTAL_TIMEOUT $cmd_string; }"
