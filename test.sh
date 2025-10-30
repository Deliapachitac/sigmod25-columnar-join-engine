#!/bin/bash
echo "Starting the build and execution process for the tests..."

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -Wno-dev
cmake --build build -- -j $(nproc) hopscotch_tests

echo "Build completed successfully."
echo ""
echo ""
echo "Running Tests..."

./build/hopscotch_tests