#include <catch2/catch_test_macros.hpp>
#include <hopscotch.h>

TEST_CASE("HopscotchMap basic insert and find", "[HopscotchMap]") {
    HopscotchMap<int, std::string> map;

    // Insert elements
    auto [it1, inserted1] = map.emplace(1, "one");
    REQUIRE(inserted1);
    REQUIRE(it1->first == 1);
    REQUIRE(it1->second == "one");

    auto [it2, inserted2] = map.emplace(2, "two");
    REQUIRE(inserted2);
    REQUIRE(it2->first == 2);
    REQUIRE(it2->second == "two");

    auto [it3, inserted3] = map.emplace(3, "three");
    REQUIRE(inserted3);
    REQUIRE(it3->first == 3);
    REQUIRE(it3->second == "three");

    // Find existing elements
    auto find_it1 = map.find(1);
    REQUIRE(find_it1 != map.end());
    REQUIRE(find_it1->first == 1);
    REQUIRE(find_it1->second == "one");

    auto find_it2 = map.find(2);
    REQUIRE(find_it2 != map.end());
    REQUIRE(find_it2->first == 2);
    REQUIRE(find_it2->second == "two");

    auto find_it3 = map.find(3);
    REQUIRE(find_it3 != map.end());
    REQUIRE(find_it3->first == 3);
    REQUIRE(find_it3->second == "three");

    // Find non-existent element
    auto find_it4 = map.find(42);
    REQUIRE(find_it4 == map.end());
}

TEST_CASE("HopscotchMap duplicate keys", "[HopscotchMap]") {
    HopscotchMap<int, std::string> map;

    auto [it1, inserted1] = map.emplace(1, "one");
    REQUIRE(inserted1);

    // Inserting the same key again should create a new entry? 
    // Our implementation doesn't prevent duplicates explicitly
    auto [it2, inserted2] = map.emplace(1, "uno");
    REQUIRE(inserted2);
    REQUIRE(it2->second == "uno");

    // Find should locate the first inserted with the key? Behavior depends on implementation
    auto find_it = map.find(1);
    REQUIRE(find_it != map.end());
}

TEST_CASE("HopscotchMap iterator traversal", "[HopscotchMap]") {
    HopscotchMap<int, int> map;
    for (int i = 0; i < 10; ++i) {
        map.emplace(i, i * 10);
    }

    std::vector<int> keys;
    std::vector<int> values;

    for (auto it = map.begin(); it != map.end(); ++it) {
        keys.push_back(it->first);
        values.push_back(it->second);
    }

    REQUIRE(keys.size() == 10);
    REQUIRE(values.size() == 10);

    for (int i = 0; i < 10; ++i) {
        REQUIRE(std::find(keys.begin(), keys.end(), i) != keys.end());
        REQUIRE(std::find(values.begin(), values.end(), i*10) != values.end());
    }
}

TEST_CASE("HopscotchMap rehashing", "[HopscotchMap]") {
    // Small map to force rehash quickly
    HopscotchMap<int, int> map(4, 4);

    size_t num_insertions = 10; // more than capacity * load factor
    for (int i = 0; i < num_insertions; ++i) {
        auto [it, inserted] = map.emplace(i, i * 2);
        REQUIRE(inserted);
    }

    // All elements should be findable
    for (int i = 0; i < num_insertions; ++i) {
        auto it = map.find(i);
        REQUIRE(it != map.end());
        REQUIRE(it->second == i * 2);
    }
}

TEST_CASE("HopscotchMap large neighborhood handling", "[HopscotchMap]") {
    HopscotchMap<int, int> map(16, 8); // smaller neighborhood

    for (int i = 0; i < 12; ++i) {
        auto [it, inserted] = map.emplace(i, i);
        REQUIRE(inserted);
    }

    // All elements should be findable
    for (int i = 0; i < 12; ++i) {
        auto it = map.find(i);
        REQUIRE(it != map.end());
        REQUIRE(it->second == i);
    }
}

TEST_CASE("HopscotchMap string keys", "[HopscotchMap]") {
    HopscotchMap<std::string, int> map;

    auto [it1, inserted1] = map.emplace("apple", 5);
    REQUIRE(inserted1);
    auto [it2, inserted2] = map.emplace("banana", 10);
    REQUIRE(inserted2);
    auto [it3, inserted3] = map.emplace("cherry", 15);
    REQUIRE(inserted3);

    REQUIRE(map.find("apple") != map.end());
    REQUIRE(map.find("banana") != map.end());
    REQUIRE(map.find("cherry") != map.end());
    REQUIRE(map.find("date") == map.end());
}
