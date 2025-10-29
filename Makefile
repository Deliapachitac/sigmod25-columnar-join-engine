# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# Include paths (headers in root, include/, and tests/)
INCLUDES = -I. -Iinclude -Itests

# Test target and sources
TEST = test_cuckoo
TEST_SRC = tests/cuckoo_test.cpp
HEADERS = include/cuckoo_hash.h

# Default rule: build and run tests
all: run_test

# Build test binary
$(TEST): $(TEST_SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TEST) $(TEST_SRC)

# Run tests
run_test: $(TEST)
	./$(TEST)

# Clean up binaries
clean:
	rm -f $(TEST)

.PHONY: all run_test clean