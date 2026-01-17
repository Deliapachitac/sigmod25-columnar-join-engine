#!/bin/bash
echo "Starting the build and execution process..."

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -Wno-dev
cmake --build build -- -j $(nproc) leaderboard

echo "Build completed successfully."
echo ""
echo ""
echo "Running..."

./build/fast plans.json