#!/bin/bash
echo "Starting the build and execution process for the tests..."

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -Wno-dev
cmake --build build -- -j $(nproc) Integration_tests
cmake --build build -- -j $(nproc) unchained_ht_tests

echo "Build completed successfully."
echo ""
echo ""
echo "Running Tests..."

./build/Integration_tests
./build/unchained_ht_tests
