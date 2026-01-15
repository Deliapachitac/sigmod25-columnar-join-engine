#!/bin/bash
echo "Starting the build and execution process for the tests..."

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -Wno-dev
cmake --build build --target Integration_tests -j $(nproc)

echo "Build completed successfully."
echo ""
echo ""
echo "Running Tests..."

./build/Integration_tests
./build/unchained_ht_tests
