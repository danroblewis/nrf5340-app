#!/bin/bash

# Build script for nRF5340 project
# This script builds from the working NCS installation

echo "Building nRF5340 project..."

# Set ccache environment variables for optimal performance
export CCACHE_DIR=~/.ccache
export CCACHE_MAXSIZE=10G
export CCACHE_COMPRESS=1
export CCACHE_COMPRESSLEVEL=6
export CCACHE_SLOPPINESS=file_macro,time_macros,include_file_mtime,include_file_ctime

# Build from NCS directory with project source
cd $ZEPHYR_BASE/../ && \
west build -p always -b nrf5340dk_nrf5340_cpuapp -s /Users/danroblewis/projects/my5340-app && \
echo "Build successful!" && \
echo "To flash, run: cd $ZEPHYR_BASE/../ && west flash" && \
echo "Build successful!"

cd $ZEPHYR_BASE/../ && west flash && \
echo "Flash successful!"
