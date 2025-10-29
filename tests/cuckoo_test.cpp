#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "cuckoo_hash.h"
#include <array>

// Checks both constructors 
TEST_CASE("Testing the default CuckooMap constructor", "[cuckoo_map]") {
    cuckoo_map<int32_t, int32_t> cm;

    REQUIRE(cm.get_capacity() == 16);
    REQUIRE(cm.empty() == true);
    for(size_t i=0; i < 16; i++){
        REQUIRE(cm.get_is_empty(1,i) == true);
        REQUIRE(cm.get_is_empty(2,i) == true);
    }
}

TEST_CASE("Testing the custom capacity constructor", "[cuckoo_map]") {
    cuckoo_map<int32_t, int32_t> cm(32);
    
    REQUIRE(cm.empty() == true);
    for(size_t i=0; i < cm.size(); i++){
        REQUIRE(cm.get_is_empty(1,i) == true);
        REQUIRE(cm.get_is_empty(2,i) == true);
    }
}

// Multiple tests for insertion collision , rehashing and detecting cycles
TEST_CASE("Testing the insertion with collisions", "[cuckoo_map]") {
    cuckoo_map<int32_t, int32_t> cm;

    // Because we insert 20 elements while the capacity is 16 rehash should occur
    // So the capacity should double
    for (size_t i = 0; i < 20; ++i)
        cm.emplace(i, i + 100);

    REQUIRE_FALSE(cm.empty());
    REQUIRE(cm.size() == 20);
    
    std::array<bool, 20> found{};
    for (size_t i = 0; i < cm.get_capacity(); ++i) {
        if (!cm.get_is_empty(1, i)) {
            int val = cm.get_value(1, i);
            int idx = val - 100;
            REQUIRE(idx >= 0 );
            found[idx] = true;
        }
        if (!cm.get_is_empty(2, i)) {
            int val = cm.get_value(2, i);
            int idx = val - 100;
            REQUIRE(idx >= 0 );
            found[idx] = true;
        }
    }
    
    // We insert more elements to test the collision handling
    cm.emplace(33, 133);
    cm.emplace(67, 167);
    cm.emplace(99, 199);
    cm.emplace(145, 245);
    cm.emplace(177, 300);
    cm.print();

    REQUIRE(cm.size() == 25);

    
}


TEST_CASE("Rehash when we have a cycle", "[cuckoo_map]") {
    cuckoo_map<int32_t, std::string> cm(8);

    size_t temp_capacity = cm.get_capacity();
    printf("Initial capacity: %zu\n", temp_capacity);
    
    // These will cause h1 collisions (key % 9)
    cm.emplace(20, "Alice"); //h1(20) = 1 h2(20) = 24
    cm.emplace(52, "Charlie"); // h1(52) = 1 h2(52) = 24
    cm.emplace(17, "Eve"); 

    cm.print();
    

    // This insertion will cause a cycle and trigger rehash
    cm.emplace(84, "Grace"); //h1(84) = 1 h2(84) = 24
    cm.print();

    REQUIRE(temp_capacity * 2 == cm.get_capacity()); // Capacity should double after rehash


    
}

TEST_CASE("Testing the find function", "[cuckoo_map]") {
    
    cuckoo_map<int32_t, int32_t> cm(50);
    for (int i = 0; i < 40; ++i)
        cm.emplace(i, i + 100);

    // Verify that all inserted elements can be found and the value is correct
    for (int i = 0; i < 40; ++i) {
        auto find_it = cm.find(i);
        REQUIRE(find_it != cm.end());
        REQUIRE(find_it->first == i);
        REQUIRE(find_it->second == i + 100);
    }

    // Verify that a non-existent key is not found
    auto not_found_it = cm.find(1000);
    REQUIRE(not_found_it == cm.end());
}

// Duplicate key insertion test 
TEST_CASE("Duplicate key handling", "[cuckoo_map]") {
    cuckoo_map<int32_t, std::string> cm(4);

    cm.emplace(7, "First");
    REQUIRE_FALSE(cm.emplace(7, "Duplicate").second); // Should fail due to duplicate key

    
    REQUIRE(cm.size() == 1);
    auto it = cm.find(7);
    REQUIRE(it != cm.end());
    REQUIRE(it->second == "First");
}

TEST_CASE("Clear map", "[cuckoo_map]") {
    cuckoo_map<int32_t, int32_t> cm;
    for (int i = 0; i < 20; ++i)
        cm.emplace(i, i * 10);

    REQUIRE_FALSE(cm.empty());
    REQUIRE(cm.size() == 20);

    cm.clear();
    REQUIRE(cm.empty());
    REQUIRE(cm.size() == 0);

    for (size_t i = 0; i < cm.get_capacity(); ++i)
        REQUIRE(cm.get_is_empty(1, i));
}
