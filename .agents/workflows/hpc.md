---
description: Remote HPC automation workflow (Sync, Build, Run, Fetch)
---

# 🌐 HPC Remote Automation

This workflow automates the cycle of running large-scale experiments on a remote HPC cluster.

## 1. Configuration
In `Taskfile.yml`, ensure the following variables are set:
- `HPC_HOST`: The SSH alias for the cluster (e.g., `login`).
- `HPC_REMOTE_DIR`: The directory on the cluster where the project will reside.

## 2. Execution Workflow

To run the complete cycle, use the "full run" task:
```bash
task hpc:full_run
```

### Individual Steps:

| Task | Command | Description |
|------|---------|-------------|
| **Sync** | `task hpc:sync` | Pushes local changes to the cluster via `rsync`. |
| **Build** | `task hpc:build` | Compiles the project on the HPC using `hpc_build.sh`. |
| **Run** | `task hpc:run:depsynt` | Submits synthesis Slurm jobs on the cluster. |
| **Monitor** | `task hpc:monitor` | Waits until all Slurm jobs are complete by polling `squeue`. |
| **Fetch** | `task hpc:fetch` | Downloads the `tasks_output/` results back to your machine. |
| **Summary** | `task hpc:summary` | Aggregates the remote JSON results into local CSVs for all tools. |
| **Visualize** | `task hpc:visualize`| Generates Cactus plots and prepares the data for Jupyer analysis. |

## ⚠️ Important Notes
- **Submodules**: `hpc:build` automatically attempts to init/update submodules.
- **Exclusions**: The sync task excludes `.git`, `libs/`, and local `tasks_output/` to keep the transfer lightweight.
- **Slurm**: The monitor task looks specifically for jobs named `depsynt` or `find_de`.

## 📈 Analysis of HPC Results

After running `task hpc:fetch` and `task hpc:summary`, your results will be located in `./tasks_output_hpc/`.

To use the local visualization tools (like `task cactus` or the Jupyter notebook) with HPC data:
1.  **Backup** your local `tasks_output/` directory if needed.
2.  **Move/Copy** the HPC results: `cp -r ./tasks_output_hpc/* ./tasks_output/`.
3.  **Run Analysis**: Now `task cactus` and other local scripts will pick up the cluster data.
