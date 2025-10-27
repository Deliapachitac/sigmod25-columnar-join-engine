#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "cuckoo_hash.h"

// Checks both constructors 
TEST_CASE("Testing the default CuckooMap constructor", "[cuckoo_map]") {
    cuckoo_map<int, int> cm;

    REQUIRE(cm.get_capacity() == 16);
    REQUIRE(cm.empty() == true);
    for(int i=0; i < 16; i++){
        REQUIRE(cm.get_is_empty(1,i) == true);
        REQUIRE(cm.get_is_empty(2,i) == true);
    }
}

TEST_CASE("Testing the custom capacity constructor", "[cuckoo_map]") {
    cuckoo_map<int, int> cm{32};
    
    REQUIRE(cm.get_capacity() == static_cast<size_t>((32/0.5)+1));
    REQUIRE(cm.empty() == true);
    for(size_t i=0; i < cm.size(); i++){
        REQUIRE(cm.get_is_empty(1,i) == true);
        REQUIRE(cm.get_is_empty(2,i) == true);
    }
}

// Multiple tests for insertion collision , rehashing and detecting cycles
TEST_CASE("Testing the insertion with collisions and rehashing(without cycles)", "[cuckoo_map]") {
    cuckoo_map<int, int> cm;

    REQUIRE(cm.get_capacity() == 16);

    // Because we insert 20 elements while the capacity is 16 rehash should occur
    // So the capacity should double
    for (int i = 0; i < 20; ++i)
        REQUIRE(cm.insert(i, i + 100));

    REQUIRE_FALSE(cm.empty());
    REQUIRE(cm.size() == 20);
    REQUIRE(cm.get_capacity() == 32);
    
    // We insert more elements to test the collision handling
    REQUIRE(cm.insert(33, 133));
    REQUIRE(cm.insert(67, 167));
    REQUIRE(cm.insert(99, 199));
    REQUIRE(cm.insert(145, 245));
    REQUIRE(cm.insert(177, 300));
    cm.print();

    REQUIRE(cm.size() == 25);
   
    // Verify that all inserted elements can be found and the value is correct
    int out = 0;
    for (int i = 0; i < 20; ++i) {
        REQUIRE(cm.find(i, out));
        REQUIRE(out == i + 100);
    }

    REQUIRE(cm.find(33, out));
    REQUIRE(out == 133);

    REQUIRE(cm.find(67, out));
    REQUIRE(out == 167);

    REQUIRE(cm.find(99, out));
    REQUIRE(out == 199);

    REQUIRE(cm.find(145, out));
    REQUIRE(out == 245);

    REQUIRE(cm.find(177, out));
    REQUIRE(out == 300);
}

TEST_CASE("Testing the insertion to handle cycles ", "[cuckoo_map]") {
    cuckoo_map<int, std::string> cm(4);

    REQUIRE(cm.get_capacity() == 9); // 4/0.5 + 1 = 9

    
    // These will cause h1 collisions (key % 9)
    REQUIRE(cm.insert(1, "Alice")); //h1(1) = 1 h2(1) = 0
    REQUIRE(cm.insert(9, "Charlie"));// h1(9) = 1 h2(9) = 2
    REQUIRE(cm.insert(82, "Eve")); //h1(82) = 1 h2(82) = 0

    REQUIRE(cm.get_capacity() == 9);
    

    // This insertion will cause a cycle and trigger rehash
    REQUIRE(cm.insert(163, "Grace")); //h1(163) = 1 h2(163) = 0
    REQUIRE(cm.get_capacity() == 18); // Capacity should double after rehash

    cm.print();

    
}

// Duplicate key insertion test 
TEST_CASE("Duplicate key handling", "[cuckoo_map]") {
    cuckoo_map<int, std::string> cm(4);

    REQUIRE(cm.insert(7, "First"));
    REQUIRE_FALSE(cm.insert(7, "Duplicate")); // Should fail due to duplicate key

    // When we search for key 7 we should get "First" not "Duplicate"
    std::string out_str;
    REQUIRE(cm.find(7, out_str));
    REQUIRE(out_str == "First");
    REQUIRE_FALSE(out_str == "Duplicate");
}
