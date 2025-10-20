#include <catch2/catch_test_macros.hpp>

#include <robin_hood.h>
#include <array>
//Checks the two different constructors and checks whether all buckets have been initialized
TEST_CASE("Default Hashmap Constructor","[robin_hood]"){
    rh_map<int32_t,int32_t> rh;
    REQUIRE( rh.size() == 16 );
    REQUIRE( rh.empty() == true );
    for (int i = 0; i < rh.size(); i++){
        REQUIRE(rh.get_is_empty(i) == true);
    }

}

TEST_CASE("Size specified Hashmap constructor", "[robin_hood]"){
    rh_map<int32_t,int32_t> rh{32};
    REQUIRE( rh.size() == static_cast<size_t>((32/0.75)+1) );  
    REQUIRE( rh.empty() == true );
    for (int i = 0; i < rh.size(); i++){
        REQUIRE(rh.get_is_empty(i) == true);
    }
}

//Tests if insert works, emplace just uses insert internally so we use this

TEST_CASE("Insert element", "[robin_hood]"){
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
    REQUIRE(rh.empty() == false);
    std::array<bool, 9> found;
    for (int i = 0; i < rh.size(); i++){
        if(rh.get_is_empty(i) == false){
            found[rh.get_v(i) - 19] = true;
            if(rh.get_psl(i) > rh.get_psl((i+1)%rh.size())){
                FAIL("Robin Hood property violated");
            }
        }
    }
    std::array<bool,9> ground_truth ;
    ground_truth.fill(true);
    REQUIRE(found == ground_truth);
    auto result = rh.emplace(1,100);
    REQUIRE(result.second == false);
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
