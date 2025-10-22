#include <catch2/catch_test_macros.hpp>
#include <robin_hood.h>
#include <array>
//Checks the two different constructors and checks whether all buckets have been initialized
TEST_CASE("Default Hashmap Constructor","[robin_hood]"){
    rh_map<int32_t,int32_t> rh;
    REQUIRE( rh.get_capacity() == 16 );
    REQUIRE( rh.empty() == true );
    for (int i = 0; i < rh.size(); i++){
        REQUIRE(rh.get_is_empty(i) == true);
    }

}

TEST_CASE("Size specified Hashmap constructor", "[robin_hood]"){
    rh_map<int32_t,int32_t> rh{32};
    REQUIRE( rh.get_capacity() == static_cast<size_t>((32/0.75)+1) );  
    REQUIRE( rh.empty() == true );
    for (int i = 0; i < rh.size(); i++){
        REQUIRE(rh.get_is_empty(i) == true);
    }
}

//Tests if insert works, emplace just uses insert internally so we use this

TEST_CASE("Insert element", "[robin_hood]") {
    rh_map<int32_t, int32_t> rh;
    for (int i = 1; i <= 9; ++i)
        rh.emplace(i, i + 19);

    REQUIRE_FALSE(rh.empty());

    std::array<bool, 9> found{};
    int prev_psl = -1;

    for (int i = 0; i < rh.size(); ++i) {
        if (!rh.get_is_empty(i)) {
            int val = rh.get_v(i);
            int idx = val - 19;
            REQUIRE(idx >= 0);
            REQUIRE(idx < 9);
            found[idx] = true;

            int curr_psl = rh.get_psl(i);
            if (prev_psl != -1 && curr_psl < prev_psl) {
                FAIL("Robin Hood property violated");
            }
            prev_psl = curr_psl;
        }
    }

    std::array<bool, 9> ground_truth;
    ground_truth.fill(true);
    REQUIRE(found == ground_truth);

    auto result = rh.emplace(1, 100);
    REQUIRE_FALSE(result.second);
}

TEST_CASE("Find element", "[robin_hood]"){
    rh_map<int32_t,int32_t> rh;
    rh.emplace(1,20);
    rh.emplace(2,21);
    rh.emplace(3,22);
    rh.emplace(4,23);
    rh.emplace(5,24);
    rh.emplace(6,25);
    rh.emplace(7,26);
    rh.emplace(8,27);
    rh.emplace(9,28);
    auto it = rh.find(1);
    REQUIRE(it->second == 20);
    REQUIRE(it->first == 1);
    it = rh.find(10);
    REQUIRE(it == rh.end());
}
