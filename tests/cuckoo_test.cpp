#include <catch2/catch_test_macros.hpp>
#include <cuckoo_hash.h>
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
    cuckoo_map<int32_t, int32_t> cm(25);
    
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
    

    //Check that the key 177 is in the hash map 
    auto it = cm.find(177);
    REQUIRE(it != cm.end());
    REQUIRE(it->second == 300);

    REQUIRE(cm.size() == 25);

    
}

TEST_CASE("Rehash when we have a cycle", "[cuckoo_map]") {
    cuckoo_map<int32_t, std::string> cm(8);

    size_t temp_capacity = cm.get_capacity();
    
    // These will cause h1 collisions (key % 9)
    cm.emplace(20, "Alice"); //h1(20) = 1 h2(20) = 24
    cm.emplace(52, "Charlie"); // h1(52) = 1 h2(52) = 24
    cm.emplace(17, "Eve"); 

    // This insertion will cause a cycle and trigger rehash
    cm.emplace(84, "Grace"); //h1(84) = 1 h2(84) = 24

    REQUIRE(temp_capacity * 2 == cm.get_capacity()); // Capacity should double after rehash
    
}

// Tests for the find function
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

// Clear function test
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

// Iterator  tests
TEST_CASE("Iterator traversal correctness", "[cuckoo_map]") {
    cuckoo_map<int32_t, int32_t> cm(20);
    std::array<int, 5> keys = {1, 3, 5, 7, 9};
    for (auto k : keys) cm.emplace(k, k * 10);

    std::vector<int> collected;
    for (auto it = cm.begin(); it != cm.end(); ++it)
        collected.push_back(it->first);

    for (auto k : keys)
        REQUIRE(std::find(collected.begin(), collected.end(), k) != collected.end());
    REQUIRE(collected.size() == 5);
}

// Testing with different key types
TEST_CASE("String keys", "[cuckoo_map]") {
    cuckoo_map<std::string, int> cm;
    cm.emplace("peter", 50);
    cm.emplace("anna", 100);
    cm.emplace("gary", 200);
    cm.emplace("sophie", 560);

    REQUIRE(cm.find("peter")->second == 50);
    REQUIRE(cm.find("anna")->second == 100);
    REQUIRE(cm.find("gary")->second == 200);
    REQUIRE(cm.find("sophie")->second == 560);
    REQUIRE(cm.find("delia") == cm.end());
}

TEST_CASE("Double keys", "[cuckoo_map]") {
    cuckoo_map<double, std::string> cm;
    cm.emplace(1.2, "chair");
    cm.emplace(35.5, "shoe");
    cm.emplace(54.6, "desk");

    REQUIRE(cm.find(1.2)->second == "chair");
    REQUIRE(cm.find(35.5)->second == "shoe");
    REQUIRE(cm.find(54.6)->second == "desk");
    REQUIRE(cm.find(0.0) == cm.end());
}

TEST_CASE("Int64 keys", "[cuckoo_map]") {
    cuckoo_map<int64_t, int> cm;
    cm.emplace(50000000000LL, 67);
    cm.emplace(70000000000LL, 98);
    cm.emplace(90000000000LL, 35);

    REQUIRE(cm.find(50000000000LL)->second == 67);
    REQUIRE(cm.find(70000000000LL)->second == 98);
    REQUIRE(cm.find(90000000000LL)->second == 35);
    REQUIRE(cm.find(40000000000LL) == cm.end());
}

