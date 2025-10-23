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
    rh_map<int32_t, int32_t> rh(200);
    for (int i = 0; i < 60; ++i)
        rh.emplace(i, i + 19);

    REQUIRE_FALSE(rh.empty());

    std::array<bool, 60> found{};
    int prev_psl = -1;

    for (int i = 0; i < rh.get_capacity(); ++i) {
        if (!rh.get_is_empty(i)) {
            int val = rh.get_v(i);
            int idx = val - 19;
            REQUIRE(idx >= 0);
            found[idx] = true;
            int curr_psl = rh.get_psl(i);
            if (prev_psl != -1 && curr_psl < prev_psl) {
                FAIL("Robin Hood property violated");
            }
            prev_psl = curr_psl;
        }
    }

    std::array<bool, 60> ground_truth;
    ground_truth.fill(true);
    REQUIRE(found == ground_truth);
    REQUIRE(rh.size() == 60);
    auto result = rh.emplace(4, 100);
    REQUIRE_FALSE(result.second);
    REQUIRE(rh.size() == 60);
}

TEST_CASE("Find element", "[robin_hood]"){
    rh_map<int32_t,int32_t> rh(60);
    for (int i = 0; i < 60; ++i)
        rh.emplace(i, i + 19);
    for(int i = 0; i < 60  ; i++){
        auto it = rh.find(i);
        REQUIRE(it->second == 19+i);
        REQUIRE(it->first == i);
    }
    auto it = rh.find(10);
    REQUIRE(it == rh.end());
}

//TODO: Test robin hood hash psl
