# Running DepSynt on HPC

The workflow is exactly as you intuited: **Clone -> Build -> Run**.

However, because you cannot use `sudo` or `build.sh` on an HPC, you should follow these specific instructions.

## 1. Clone
Clone the repository to your work directory on the cluster.

```bash
git clone <repo_url> depsynt
cd depsynt
```

## 2. Build
You need to compile the `find_dependencies` tool manually or using the provided helper script.

**Prerequisites:**
Most clusters use Environment Modules. Load them first:
```bash
module load gcc cmake boost
# If 'boost' module is not available, the helper script will try to unpack the one from 'packages/'
```

**Running the Build:**
I have provided `hpc_build.sh` which handles the complexity of extracting the `.deb` dependencies (Spot, JSON, etc.) into a local folder and pointing CMake to them.

```bash
chmod +x hpc_build.sh
./hpc_build.sh
```

If successful, you will see `find_dependencies` in the current directory.

## 3. Run
Submit the job to Slurm using the script generated earlier.

```bash
sbatch run_find_dependencies.slurm
```

### Monitoring
- Check queue: `squeue -u $USER`
- Check output: `cat find_deps_*.out`
