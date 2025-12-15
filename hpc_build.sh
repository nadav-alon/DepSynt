#!/bin/bash
# HPC Build Helper Script
# Use this to build the tool on an HPC where you don't have sudo access.

set -e

# Directory to install local dependencies
SEARCH_PATH="$(pwd)/hpc_deps"
mkdir -p "$SEARCH_PATH"

echo "=== 1. Setting up Local Dependencies in $SEARCH_PATH ==="

extract_deb() {
    local deb_dir=$1
    echo "Extracting .deb files from $deb_dir..."
    for deb in "$deb_dir"/*.deb; do
        if [ -f "$deb" ]; then
             # Extract into a temporary spot then move to SEARCH_PATH
             # to handle the internal structure of the deb (usually usr/...)
             # We use 'ar' to extract the deb archive components
             cd "$SEARCH_PATH"
             ar x "$deb"
             # Extract data.tar.* (could be xz, gz, etc)
             if [ -f data.tar.xz ]; then
                 tar -xf data.tar.xz
             elif [ -f data.tar.gz ]; then
                 tar -xf data.tar.gz
             elif [ -f data.tar ]; then
                 tar -xf data.tar
             elif [ -f data.tar.zst ]; then
                 # Requires zstd, might not be available, but standard ubuntu debs are usually xz
                 tar -I zstd -xf data.tar.zst
             fi
             # Cleanup intermediate files
             rm -f control.tar* data.tar* debian-binary
             cd - > /dev/null
        fi
    done
}

# Extract Dependencies provided in the repo
# Try provided boost packages (fallback if module load boost fails)
if [ -z "$BOOST_ROOT" ] && ! ldconfig -p | grep libboost_program_options > /dev/null 2>&1; then
    echo "Boost seems missing from system/modules. Attempting to extract from packages/..."
    extract_deb "$(pwd)/packages/boost"
else
    echo "Boost found on system (or skipped extraction). If build fails, try 'module load boost'."
fi

extract_deb "$(pwd)/packages/spot"
extract_deb "$(pwd)/packages/nlohmann-json"

# Build ABC
echo "=== 2. Building ABC Library ==="
cd libs/abc
make -j4 ABC_USE_NO_READLINE=1 ABC_USE_PIC=1 libabc.so
# Link ABC to our local deps folder so CMake finds it
# The CMakeLists looks for libabc.so. We can put it in SEARCH_PATH/lib (or root)
mkdir -p "$SEARCH_PATH/lib" "$SEARCH_PATH/include"
cp libabc.so "$SEARCH_PATH/lib/"
# Copy headers if needed? CMakeLists says include_directories(./libs/abc/src) so might not need copy.
cd ../..

echo "=== 3. Running CMake ==="
# We assume the user has loaded a cmake module or has it installed
# We pass CMAKE_PREFIX_PATH to find our locally extracted libs
# The extracted debs usually put things in usr/lib/x86_64-linux-gnu or usr/lib
# We add $SEARCH_PATH and $SEARCH_PATH/usr to prefix path
cmake . \
    -DCMAKE_PREFIX_PATH="$SEARCH_PATH;$SEARCH_PATH/usr;$SEARCH_PATH/usr/lib/x86_64-linux-gnu" \
    -DCMAKE_INCLUDE_PATH="$SEARCH_PATH/usr/include" \
    -DCMAKE_LIBRARY_PATH="$SEARCH_PATH/usr/lib/x86_64-linux-gnu;$SEARCH_PATH/usr/lib"

echo "=== 4. compiling ==="
make -j4 find_dependencies

echo "=== Build Complete ==="
echo "You can now submit the job using: sbatch run_find_dependencies.slurm"
