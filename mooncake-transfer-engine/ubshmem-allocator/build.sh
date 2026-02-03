#!/bin/bash

set -e

# Get output directory from command line argument, default to current directory
OUTPUT_DIR=${1:-.}

# Get include directories from second argument (if provided)
INCLUDE_LIST=""
if [ $# -ge 2 ]; then
    INCLUDE_LIST=${2}
fi

# Add include directory for cuda (relative to build.sh location)
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
INCLUDE_LIST="${INCLUDE_LIST:+${INCLUDE_LIST} }${SCRIPT_DIR}/../include"

# Process include directories into flags
INCLUDE_FLAGS=""
if [ -n "$INCLUDE_LIST" ]; then
    INCLUDE_FLAGS=$(echo "$INCLUDE_LIST" | tr ' ' '\n' | sed 's/^/-I/' | paste -sd' ' -)
fi

echo "Building ubshmem fabric allocator to: $OUTPUT_DIR"
# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

CPP_FILE=$(dirname $(readlink -f $0))/ubshmem_fabric_allocator.cpp

# Find Ascend toolkit
ASCEND_TOOLKIT_ROOT=$(find /usr/local/Ascend/ascend-toolkit/latest -maxdepth 1 -type d -name "*-linux" 2>/dev/null | head -n 1)
if [ -z "$ASCEND_TOOLKIT_ROOT" ]; then
    echo "Error: Cannot find Ascend toolkit in /usr/local/Ascend/ascend-toolkit/latest"
    exit 1
fi

ASCEND_INCLUDE_DIR="${ASCEND_TOOLKIT_ROOT}/include"
ASCEND_LIB_DIR="${ASCEND_TOOLKIT_ROOT}/lib64"

echo "Using Ascend toolkit at: $ASCEND_TOOLKIT_ROOT"

g++ "$CPP_FILE" \
    -o "$OUTPUT_DIR/ubshmem_fabric_allocator.so" \
    -shared -fPIC \
    -std=c++11 \
    -I"$ASCEND_INCLUDE_DIR" \
    ${INCLUDE_FLAGS} \
    -L"$ASCEND_LIB_DIR" \
    -lascendcl \
    -DUSE_ASCEND=1

if [ $? -eq 0 ]; then
    echo "Successfully built ubshmem_fabric_allocator.so in $OUTPUT_DIR"
else
    echo "Failed to build ubshmem_fabric_allocator.so"
    exit 1
fi
