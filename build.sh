#!/bin/bash

# Build script for nRF5340 project
# This script builds from the working NCS installation

echo "Building nRF5340 project..."

# Build from NCS directory with project source
cd ~/ncs && \
west build -p always -b nrf5340dk_nrf5340_cpuapp -s /Users/danroblewis/projects/my5340-app && \
echo "Build successful!" && \
echo "To flash, run: cd ~/ncs && west flash"