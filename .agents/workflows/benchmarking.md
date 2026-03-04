---
description: How to handle measuring and benchmarking in the DepSynt project
---

# Measuring and Benchmarking Workflow

The project uses a structured measurement system to track the performance and characteristics of the synthesis process. This is primarily implemented in `src/utils/synt_measure.h` and `src/utils/synt_measure.cpp`.

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

### 2. Running Benchmarks
#### Local Execution
Use `run-benchmarks.py` for small-scale local tests:
```bash
python run-benchmarks.py --benchmarks "mux,shift" --timeout 5000 --output-csv results.csv
```

#### Cluster Execution (Slurm)
The `Taskfile.yml` contains tasks for submitting large-scale jobs to a Slurm cluster:
- `task depsynt`: Runs the main tool on the generated benchmarks.
- `task find_dependencies`: Only runs the dependency finder.
- `task depsynt:measure`: Runs the tool with extra measurement flags enabled.

These tasks use `scripts/slurm_task_gen.py` to create submission scripts.

### 3. Summarizing Results
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
