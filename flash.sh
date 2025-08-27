#!/bin/bash

# Flash script for nRF5340 project

echo "Flashing nRF5340 project..."

# Flash from NCS directory
cd ~/ncs && west flash && \
echo "Flash successful!"
