SEARCH_PATH="$(pwd)/hpc_deps"
# Setup environment for HPC compute nodes
module load boost 2>/dev/null || echo "Module boost not found, relying on local hpc_deps"
export LD_LIBRARY_PATH="$SEARCH_PATH/usr/lib/x86_64-linux-gnu:$SEARCH_PATH/usr/lib:$SEARCH_PATH/lib:$LD_LIBRARY_PATH"
export PATH="$(pwd)/.bin:$PATH"
