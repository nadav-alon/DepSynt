#!/bin/bash
#SBATCH --job-name=build_depsynt
#SBATCH --output=build_%j.out
#SBATCH --error=build_%j.err
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
# Try provided boost packages (fallback if module load boost fails)
if [ -z "$BOOST_ROOT" ] && ! ldconfig -p | grep libboost_program_options > /dev/null 2>&1; then
    echo "Boost seems missing from system/modules. Attempting to extract from packages/..."
    extract_deb "$(pwd)/packages/boost" "$SEARCH_PATH/.boost_extracted"
else
    echo "Boost found on system (or skipped extraction). If build fails, try 'module load boost'."
fi

extract_deb "$(pwd)/packages/spot" "$SEARCH_PATH/.spot_extracted"
extract_deb "$(pwd)/packages/nlohmann-json" "$SEARCH_PATH/.json_extracted"

# Build ABC
echo "=== 2. Building ABC Library ==="
if [ -z "$(ls -A libs/abc)" ]; then
    echo "Error: libs/abc is empty. Please run: git submodule update --init --recursive"
    exit 1
fi

mkdir -p "$SEARCH_PATH/lib" "$SEARCH_PATH/include"

if [ -f "libs/abc/libabc.so" ] && [ -f "$SEARCH_PATH/lib/libabc.so" ]; then
    echo "ABC library already exists, skipping build..."
else
    cd libs/abc
    make -j4 ABC_USE_NO_READLINE=1 ABC_USE_PIC=1 libabc.so
    cp libabc.so "$SEARCH_PATH/lib/"
    cd ../..
fi

echo "=== 3. Cleaning and Running CMake ==="
# We add $SEARCH_PATH and $SEARCH_PATH/usr to prefix path
# On old CentOS 7 systems, we often need to link libstdc++ statically if the system's libstdc++.so is too old.
rm -rf CMakeCache.txt CMakeFiles/
cmake . \
    -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc" \
    -DCMAKE_PREFIX_PATH="$SEARCH_PATH;$SEARCH_PATH/usr;$SEARCH_PATH/usr/lib/x86_64-linux-gnu;$SEARCH_PATH/usr/lib" \
    -DCMAKE_INCLUDE_PATH="$SEARCH_PATH/usr/include" \
    -DCMAKE_LIBRARY_PATH="$SEARCH_PATH/lib;$SEARCH_PATH/usr/lib/x86_64-linux-gnu;$SEARCH_PATH/usr/lib"

echo "=== 4. compiling ==="
make -j4 find_dependencies find_input_dependencies depsynt inp_dep_synthesis

echo "=== Build Complete ==="
