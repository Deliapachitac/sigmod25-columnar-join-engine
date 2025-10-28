#include <catch2/catch_test_macros.hpp>
#include <array>
#include <algorithm>
#include <string>
#include <robin_hood.h> 

// --- Basic Constructor Tests ---

TEST_CASE("Default Hashmap Constructor", "[robin_hood]") {
    rh_map<int32_t, int32_t> rh;
    REQUIRE(rh.get_capacity() == 16);
    REQUIRE(rh.empty());

    for (size_t i = 0; i < rh.get_capacity(); i++)
        REQUIRE(rh.get_is_empty(i));
}

TEST_CASE("Size-specified Hashmap constructor", "[robin_hood]") {
    rh_map<int32_t, int32_t> rh{32};
    REQUIRE(rh.get_capacity() == 64); //Next power of 2, make sure it is not the default size
    REQUIRE(rh.empty());

    for (size_t i = 0; i < rh.get_capacity(); i++)
        REQUIRE(rh.get_is_empty(i));
}

// --- Insert Tests ---

TEST_CASE("Insert element and check Robin Hood property", "[robin_hood]") {
    rh_map<int32_t, int32_t> rh(200);
    for (int i = 0; i < 60; ++i)
        rh.emplace(i, i + 19);

    REQUIRE_FALSE(rh.empty());
    REQUIRE(rh.size() == 60);

    std::array<bool, 60> found{};
    int prev_psl = -1;

    for (size_t i = 0; i < rh.get_capacity(); ++i) {
        if (!rh.get_is_empty(i)) {
            int val = rh.get_v(i);
            int idx = val - 19;
            REQUIRE(idx >= 0);
            found[idx] = true;

            int curr_psl = rh.get_psl(i);
            if (prev_psl != -1)
                REQUIRE(curr_psl >= prev_psl); // Non-decreasing PSL (Robin Hood property)
            prev_psl = curr_psl;
        }
    }

    std::array<bool, 60> ground_truth;
    ground_truth.fill(true);
    REQUIRE(found == ground_truth);

    auto result = rh.emplace(4, 100); // duplicate
    REQUIRE_FALSE(result.second);
    REQUIRE(rh.size() == 60);
}

// --- Find Tests ---

TEST_CASE("Find element", "[robin_hood]") {
    rh_map<int32_t, int32_t> rh(60);
    for (int i = 0; i < 60; ++i)
        rh.emplace(i, i + 19);

    for (int i = 0; i < 60; i++) {
        auto it = rh.find(i);
        REQUIRE(it != rh.end());
        REQUIRE(it->first == i);
        REQUIRE(it->second == 19 + i);
    }

    auto it = rh.find(9999);
    REQUIRE(it == rh.end());
}

// --- Iterator Tests ---

TEST_CASE("Iterator traversal correctness", "[robin_hood]") {
    rh_map<int32_t, int32_t> rh(20);
    std::array<int, 5> keys = {1, 3, 5, 7, 9};
    for (auto k : keys) rh.emplace(k, k * 10);

    std::vector<int> collected;
    for (auto it = rh.begin(); it != rh.end(); ++it)
        collected.push_back(it->first);

    for (auto k : keys)
        REQUIRE(std::find(collected.begin(), collected.end(), k) != collected.end());
    REQUIRE(collected.size() == 5);
}

// --- Rehash Tests ---

TEST_CASE("Rehash when exceeding load factor", "[robin_hood]") {
    rh_map<int32_t, int32_t> rh(4);
    size_t old_cap = rh.get_capacity();

    for (int i = 0; i < 100; i++)
        rh.emplace(i, i * 2);

    REQUIRE(rh.size() == 100);
    REQUIRE(rh.get_capacity() > old_cap);

    for (int i = 0; i < 100; i++) {
        auto it = rh.find(i);
        REQUIRE(it != rh.end());
        REQUIRE(it->second == i * 2);
    }
}

// --- Clear Tests ---

TEST_CASE("Clear map", "[robin_hood]") {
    rh_map<int32_t, int32_t> rh;
    for (int i = 0; i < 20; ++i)
        rh.emplace(i, i * 10);

    REQUIRE_FALSE(rh.empty());
    REQUIRE(rh.size() == 20);

    rh.clear();
    REQUIRE(rh.empty());
    REQUIRE(rh.size() == 0);

    for (size_t i = 0; i < rh.get_capacity(); ++i)
        REQUIRE(rh.get_is_empty(i));
}

// --- String Key Tests ---

TEST_CASE("String keys", "[robin_hood]") {
    rh_map<std::string, int> rh(16);
    rh.emplace("apple", 10);
    rh.emplace("banana", 20);
    rh.emplace("cherry", 30);

    REQUIRE(rh.find("apple")->second == 10);
    REQUIRE(rh.find("banana")->second == 20);
    REQUIRE(rh.find("cherry")->second == 30);
    REQUIRE(rh.find("mango") == rh.end());
}

// --- Double Key Tests ---

TEST_CASE("Double keys", "[robin_hood]") {
    rh_map<double, std::string> rh;
    rh.emplace(3.14, "pi");
    rh.emplace(2.71, "e");
    rh.emplace(1.41, "sqrt2");

    REQUIRE(rh.find(3.14)->second == "pi");
    REQUIRE(rh.find(2.71)->second == "e");
    REQUIRE(rh.find(1.41)->second == "sqrt2");
    REQUIRE(rh.find(0.0) == rh.end());
}

// --- Stress & Collision Tests ---

TEST_CASE("Heavy insertion with collisions", "[robin_hood]") {
    rh_map<int, int> rh(8);
    for (int i = 0; i < 2000; ++i)
        rh.emplace(i, i + 1);

    REQUIRE(rh.size() == 2000);

    for (int i = 0; i < 2000; i += 500)
        REQUIRE(rh.find(i)->second == i + 1);
}
// --- Const Iterator test ---
TEST_CASE("Const iterator works", "[robin_hood]") {
    rh_map<int,int> rh;
    for (int i = 0; i < 5; ++i)
        rh.emplace(i, i * 2);

    const rh_map<int,int>& crh = rh;
    int count = 0;
    for (auto it = crh.begin(); it != crh.end(); ++it) {
        REQUIRE(it->second == it->first * 2);
        count++;
    }
    REQUIRE(count == 5);
}
