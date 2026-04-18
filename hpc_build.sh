#!/bin/bash
#SBATCH --job-name=build_depsynt
#SBATCH --output=tasks_output/build.log
#SBATCH --error=tasks_output/build.log
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=8G
#SBATCH --time=01:00:00

# HPC Build Helper Script
# Use this to build the tool on an HPC where you don't have sudo access.

set -e

# Directory to install local dependencies
SEARCH_PATH="$(pwd)/hpc_deps"
mkdir -p "$SEARCH_PATH"

echo "=== 1. Setting up Local Dependencies in $SEARCH_PATH ==="

extract_deb() {
    local deb_dir=$1
    local marker_file=$2
    if [ -f "$marker_file" ]; then
        echo "Marker $marker_file exists, skipping extraction for $deb_dir..."
        return
    fi
    echo "Extracting .deb files from $deb_dir..."
    for deb in "$deb_dir"/*.deb; do
        if [ -f "$deb" ]; then
             cd "$SEARCH_PATH"
             ar x "$deb"
             if [ -f data.tar.xz ]; then
                 tar -xf data.tar.xz
             elif [ -f data.tar.gz ]; then
                 tar -xf data.tar.gz
             elif [ -f data.tar ]; then
                 tar -xf data.tar
             elif [ -f data.tar.zst ]; then
                 tar -I zstd -xf data.tar.zst
             fi
             rm -f control.tar* data.tar* debian-binary
             cd - > /dev/null
        fi
    done
    touch "$marker_file"
}

# Extract Dependencies provided in the repo
echo "Extracting provided packages (Boost, GCC helpers, etc.) from packages/boost..."
extract_deb "$(pwd)/packages/boost" "$SEARCH_PATH/.boost_extracted"

extract_deb "$(pwd)/packages/spot" "$SEARCH_PATH/.spot_extracted"
extract_deb "$(pwd)/packages/nlohmann-json" "$SEARCH_PATH/.json_extracted"

# Build ABC
echo "=== 2. Building ABC Library ==="
if [ -z "$(ls -A libs/abc)" ]; then
    echo "Error: libs/abc is empty. Please run: git submodule update --init --recursive"
    exit 1
fi

mkdir -p "$SEARCH_PATH/lib" "$SEARCH_PATH/include"

if [ -f "libs/abc/libabc.a" ] && [ -f "$SEARCH_PATH/lib/libabc.a" ]; then
    echo "ABC static library already exists, skipping build..."
else
    cd libs/abc
    make -j4 ABC_USE_NO_READLINE=1 ABC_USE_PIC=1 libabc.a
    cp libabc.a "$SEARCH_PATH/lib/"
    cd ../..
fi

echo "=== 3. Cleaning and Running CMake ==="
# We add $SEARCH_PATH and $SEARCH_PATH/usr to prefix path
# On old CentOS 7 systems, we often need to link libstdc++ statically if the system's libstdc++.so is too old.
# Debug: List available static libraries in hpc_deps
echo "=== Debug: Searching for static libraries in hpc_deps ==="
find "$SEARCH_PATH" -name "*.a" || echo "No static libraries found in $SEARCH_PATH"
echo "========================================================="

rm -rf CMakeCache.txt CMakeFiles/
# Ensure we don't use stale binaries if build fails
rm -f find_dependencies find_input_dependencies depsynt inp_dep_synthesis simulate_aiger

cmake . \
    -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc -L$SEARCH_PATH/usr/lib/gcc/x86_64-linux-gnu/11" \
    -DCMAKE_PREFIX_PATH="$SEARCH_PATH;$SEARCH_PATH/usr;$SEARCH_PATH/usr/lib/x86_64-linux-gnu;$SEARCH_PATH/usr/lib" \
    -DCMAKE_INCLUDE_PATH="$SEARCH_PATH/usr/include" \
    -DCMAKE_LIBRARY_PATH="$SEARCH_PATH/lib;$SEARCH_PATH/usr/lib/x86_64-linux-gnu;$SEARCH_PATH/usr/lib"

echo "=== 4. Compiling ==="
make -j4 find_dependencies find_input_dependencies depsynt inp_dep_synthesis

echo "=== 5. Post-Build Diagnostics ==="
echo "Checking binary: find_input_dependencies"
if [ -f "find_input_dependencies" ]; then
    echo "--- LDD Output ---"
    ldd find_input_dependencies
    echo "--- NM (Intersects) Output ---"
    nm -C find_input_dependencies | grep intersects || echo "Symbol 'intersects' not found in binary"
    echo "--- File Type ---"
    file find_input_dependencies
else
    echo "Error: find_input_dependencies not found!"
fi

echo "--- HPC DEPS Structure ---"
ls -R "$SEARCH_PATH"
echo "================================"

echo "=== Build Complete ==="
