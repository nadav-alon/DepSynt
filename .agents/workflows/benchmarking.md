---
description: How to handle measuring and benchmarking in the DepSynt project
---

# Measuring and Benchmarking Workflow

The project uses a structured measurement system to track the performance and characteristics of the synthesis process. This is primarily implemented in `src/utils/synt_measure.h` and `src/utils/synt_measure.cpp`.

**Make sure in all implementation changes to include measurements**

## 📊 Measurement System

### 1. The `SynthesisMeasure` Class
The main class for collecting data is `SynthesisMeasure`. It tracks:
- **Timings**: Automaton construction, dependency search, synthesis of independent/dependent variables, strategy merging, and model checking.
- **Sizes**: Number of states and edges in the NBA and projected NBA.
- **Circuit Stats**: Number of inputs, outputs, latches, and gates in the resulting AIGER strategy.
- **BDD Metrics**: Average/max/min BDD sizes (if the `--measure-bdd` flag is used).

### 2. Enabling Measurements
To enable detailed measurements in the CLI tools (`depsynt`, `inp_dep_synthesis`, `find_dependencies`):
- Use `--measures-path <FILE>` to output the results as a JSON object to a specific file.
- Use `--verbose` to see high-level progress in stdout.
- Use `--measure-bdd` (or `-m`) for granular BDD statistics.

The tools are designed to dump these measures even on signals like `SIGTERM` or `SIGHUP`, which is useful for tracking performance on timeouts.

## 🚀 Benchmarking Workflow

### 1. Generating Benchmarks
Before running benchmarks, transform TLSF files into the internal text format:
```bash
task generate_benchmarks
```
This runs `scripts/tlsf_to_text.py` and stores the results in `./tasks_output/generated_benchmarks/text`.

> [!WARNING]
> **Worktree Caveat:** If you are operating in a git worktree, `task generate_benchmarks` will fail because git submodules (like `scripts/benchmarks/tlsf`) are typically only initialized in the main repository. 
> To mitigate this, **ALWAYS** generate benchmarks from the main repository path (e.g., `cd /home/cowclaw/DepSynt-1 && task generate_benchmarks`), and then use or copy the generated files from its `tasks_output/generated_benchmarks/text` folder.

### 2. Adding or Updating Benchmarks
The benchmark suite is dynamic. To add new benchmarks:
- **TLSF Source**: Add new `.tlsf` files to the appropriate subdirectory in `scripts/benchmarks/`.
- **Benchmark Families**: If adding a new category, update the `BENCHMARKS_FAMALIES` variable in `Taskfile.yml`.
- **Text Conversion**: Run `task generate_benchmarks` to ensure the new files are parsed and listed in the `tasks_output/` pool.
- **Verification**: Check `./tasks_output/generated_benchmarks/text` to verify that each `.tlsf` now has a corresponding `.txt` with the formula and variable partitions.

### 3. Running Benchmarks
#### Local Execution
Use `run-benchmarks.py` for small-scale local tests:
```bash
python run-benchmarks.py --benchmarks "mux,shift" --timeout 5000 --output-csv results.csv
```

#### Cluster Execution (Slurm / HPC)
The `Taskfile.yml` contains tasks for submitting large-scale jobs to a Slurm cluster. For automated remote workflows (Sync, Build, Run, Fetch), refer to the **[hpc.md](file:///home/cowclaw/DepSynt-1/.agents/workflows/hpc.md)** workflow.

- `task depsynt`: Runs the main tool on the generated benchmarks.
- `task find_dependencies`: Only runs the dependency finder.
- `task depsynt:measure`: Runs the tool with extra measurement flags enabled.

These tasks use `scripts/slurm_task_gen.py` to create submission scripts.

### 4. Summarizing Results
Once benchmarking is complete, aggregate the `.out` (JSON) and `.err` files into a single CSV:
```bash
task depsynt:summary
# OR
python scripts/results_summarizer.py --result-path ./tasks_output/depsynt --benchmarks-path ./tasks_output/generated_benchmarks/text --summary-output summary.csv --tool depsynt
```

## 📈 Analysis and Visualization

- **Cactus Plots**: Run `task cactus` to generate a cactus plot showing the distribution of execution times.
- **Jupyter Notebook**: Use `scripts/visualization.ipynb` for more granular data analysis, including Scatter plots and Ratio comparisons.

## 🛠 Extending Metrics

If you need to track a new metric for Input Dependency synthesis:

1.  **Strict Constraint**: You **MUST NOT** modify `src/utils/synt_measure.h` or `src/utils/synt_measure.cpp`. These are core files from the original project.
2.  **Use Subclass**: Extend the `InpDepSyntMeasure` class in `src/inputDependencies/measure.h`.
3.  **Instrument the code**:
    - Add your new fields and `start_...()`/`end_...()` methods to `InpDepSyntMeasure`.
    - Call these methods in your algorithm implementations (e.g., `src/inputDependencies/naive_algorithm.cpp`).
4.  **Update Serialization**: Override `get_json_object` in `InpDepSyntMeasure` to include your new fields while calling the base class `SynthesisMeasure::get_json_object(obj)` first.
5.  **Update Summarizer**: Add the new keys to the Python `scripts/results_summarizer.py` to ensure they appear in the final aggregated benchmarks.