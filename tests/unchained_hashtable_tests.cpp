// unchained_hashtable_tests.cpp
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <stdexcept>
#include "unchained_hashtable.h"

// Helper to finalize using the hashtable's internal pipeline
static void finalize_single_thread(unchained_ht &ht)
{
    // Step 1: Prepare global structures (directory, array allocation)
    if (ht.prepare_build()) {
        // Step 2: Process all partitions (single-threaded for tests)
        for (size_t p = 0; p < 8; ++p) {
            ht.post_process_build(p);
        }
    }
    // Step 3: Mark as built for probing
    ht.finalize_build();
}

// ------------------------------------------------------------
// --- Constructor & Initial State Tests ---
// ------------------------------------------------------------

TEST_CASE("Default constructor initializes empty, unbuilt table", "[unchained_ht]") {
    unchained_ht ht;
    REQUIRE(ht.get_tuple_count() == 0);
}

TEST_CASE("Probe before finalize throws", "[unchained_ht]") {
    unchained_ht ht;
    REQUIRE_THROWS_AS(ht.probe(42), std::runtime_error);
}

TEST_CASE("Finalize on empty table is allowed", "[unchained_ht]") {
    unchained_ht ht;
    REQUIRE_NOTHROW(ht.finalize_build());
    REQUIRE(ht.get_tuple_count() == 0);

    auto res = ht.probe(1);
    REQUIRE(res.empty());
}

// ------------------------------------------------------------
// --- Build Insert Tests ---
// ------------------------------------------------------------

TEST_CASE("Single insert then finalize", "[unchained_ht]") {
    unchained_ht ht;

    ht.build_insert(10, 123, 0);
    finalize_single_thread(ht);

    REQUIRE(ht.get_tuple_count() == 1);
    REQUIRE(ht.get_directory_size() >= 1);

    auto res = ht.probe(10);
    REQUIRE(res.size() == 1);
    REQUIRE(res[0] == 123);
}

TEST_CASE("Multiple inserts with distinct keys", "[unchained_ht]") {
    unchained_ht ht;

    for (int i = 0; i < 100; ++i)
        ht.build_insert(i, i * 10, 0);

    finalize_single_thread(ht);

    REQUIRE(ht.get_tuple_count() == 100);

    for (int i = 0; i < 100; ++i) {
        auto res = ht.probe(i);
        REQUIRE(res.size() == 1);
        REQUIRE(res[0] == static_cast<size_t>(i * 10));
    }
}

TEST_CASE("Insert after finalize throws", "[unchained_ht]") {
    unchained_ht ht;
    ht.build_insert(1, 1, 0);
    finalize_single_thread(ht);

    REQUIRE_THROWS_AS(ht.build_insert(2, 2, 0), std::runtime_error);
}

// ------------------------------------------------------------
// --- Duplicate Key Tests ---
// ------------------------------------------------------------

TEST_CASE("Duplicate keys produce multiple values", "[unchained_ht]") {
    unchained_ht ht;

    ht.build_insert(7, 100, 0);
    ht.build_insert(7, 200, 0);
    ht.build_insert(7, 300, 0);
    finalize_single_thread(ht);

    auto res = ht.probe(7);
    REQUIRE(res.size() == 3);

    std::sort(res.begin(), res.end());
    REQUIRE(res == std::vector<size_t>{100, 200, 300});
}

TEST_CASE("Duplicate and unique keys mixed", "[unchained_ht]") {
    unchained_ht ht;

    ht.build_insert(1, 10, 0);
    ht.build_insert(2, 20, 0);
    ht.build_insert(1, 11, 0);
    ht.build_insert(3, 30, 0);
    ht.build_insert(2, 21, 0);

    finalize_single_thread(ht);

    auto r1 = ht.probe(1);
    auto r2 = ht.probe(2);
    auto r3 = ht.probe(3);

    REQUIRE(r1.size() == 2);
    REQUIRE(r2.size() == 2);
    REQUIRE(r3.size() == 1);

    std::sort(r1.begin(), r1.end());
    std::sort(r2.begin(), r2.end());

    REQUIRE(r1 == std::vector<size_t>{10, 11});
    REQUIRE(r2 == std::vector<size_t>{20, 21});
    REQUIRE(r3[0] == 30);
}

// ------------------------------------------------------------
// --- Negative & Large Key Tests ---
// ------------------------------------------------------------

TEST_CASE("Negative keys work correctly", "[unchained_ht]") {
    unchained_ht ht;

    ht.build_insert(-1, 111, 0);
    ht.build_insert(-100, 222, 0);
    ht.build_insert(-1, 333, 0);
    finalize_single_thread(ht);

    auto r1 = ht.probe(-1);
    auto r2 = ht.probe(-100);

    REQUIRE(r1.size() == 2);
    REQUIRE(r2.size() == 1);

    std::sort(r1.begin(), r1.end());
    REQUIRE(r1 == std::vector<size_t>{111, 333});
    REQUIRE(r2[0] == 222);
}

TEST_CASE("Extreme int32 keys", "[unchained_ht]") {
    unchained_ht ht;

    ht.build_insert(INT32_MIN, 1, 0);
    ht.build_insert(INT32_MAX, 2, 0);
    finalize_single_thread(ht);

    auto rmin = ht.probe(INT32_MIN);
    auto rmax = ht.probe(INT32_MAX);

    REQUIRE(rmin.size() == 1);
    REQUIRE(rmax.size() == 1);
    REQUIRE(rmin[0] == 1);
    REQUIRE(rmax[0] == 2);
}

// ------------------------------------------------------------
// --- Bloom Filter / False Negative Safety ---
// ------------------------------------------------------------

TEST_CASE("Bloom filter never rejects real keys", "[unchained_ht]") {
    unchained_ht ht;

    constexpr int N = 1000;
    for (int i = 0; i < N; ++i)
        ht.build_insert(i, i + 5, 0);

    finalize_single_thread(ht);

    for (int i = 0; i < N; ++i) {
        auto res = ht.probe(i);
        REQUIRE(res.size() == 1);
        REQUIRE(res[0] == static_cast<size_t>(i + 5));
    }
}

TEST_CASE("Non-existent keys return empty result", "[unchained_ht]") {
    unchained_ht ht;

    for (int i = 0; i < 200; ++i)
        ht.build_insert(i * 2, i, 0);

    finalize_single_thread(ht);

    for (int i = 0; i < 200; ++i) {
        auto res = ht.probe(i * 2 + 1);
        REQUIRE(res.empty());
    }
}

// ------------------------------------------------------------
// --- Stress & Randomized Tests ---
// ------------------------------------------------------------

TEST_CASE("Large randomized build and probe", "[unchained_ht][stress]") {
    unchained_ht ht;

    constexpr size_t N = 50'000;
    std::vector<int32_t> keys(N);
    std::iota(keys.begin(), keys.end(), 0);

    std::mt19937 rng(123);
    std::shuffle(keys.begin(), keys.end(), rng);

    for (size_t i = 0; i < N; ++i)
        ht.build_insert(keys[i], i, 0);

    finalize_single_thread(ht);

    REQUIRE(ht.get_tuple_count() == N);
    REQUIRE(ht.get_directory_size() >= N / 0.65);

    for (size_t i = 0; i < N; ++i) {
        auto res = ht.probe(keys[i]);
        REQUIRE(res.size() == 1);
    }
}

TEST_CASE("Heavy skew: all keys identical", "[unchained_ht][stress]") {
    unchained_ht ht;

    constexpr size_t N = 10'000;
    for (size_t i = 0; i < N; ++i)
        ht.build_insert(42, i, 0);

    finalize_single_thread(ht);

    auto res = ht.probe(42);
    REQUIRE(res.size() == N);

    std::sort(res.begin(), res.end());
    REQUIRE(res.front() == 0);
    REQUIRE(res.back() == N - 1);
}

// ------------------------------------------------------------
// --- Directory & Load Factor Properties ---
// ------------------------------------------------------------

TEST_CASE("Directory size is power of two", "[unchained_ht]") {
    unchained_ht ht;

    for (int i = 0; i < 1234; ++i)
        ht.build_insert(i, i, 0);

    finalize_single_thread(ht);

    size_t dir = ht.get_directory_size();
    REQUIRE(dir != 0);
    REQUIRE((dir & (dir - 1)) == 0); // power of two
}

TEST_CASE("Finalize is idempotent", "[unchained_ht]") {
    unchained_ht ht;

    for (int i = 0; i < 10; ++i)
        ht.build_insert(i, i, 0);

    finalize_single_thread(ht);
    auto dir1 = ht.get_directory_size();
    auto cnt1 = ht.get_tuple_count();

    REQUIRE_NOTHROW(ht.finalize_build());

    REQUIRE(ht.get_directory_size() == dir1);
    REQUIRE(ht.get_tuple_count() == cnt1);
}