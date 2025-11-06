#include <catch2/catch_test_macros.hpp>
#include <hopscotch.h>

/* ==================== CONSTRUCTORS ==================== */
TEST_CASE("Basic Constructor with no parameters", "[HopscotchMap]")
{
    HopscotchMap<int, std::string> map;

    static constexpr size_t DEFAULT_NEIGHBORHOOD_LENGTH = 32;
    static constexpr size_t DEFAULT_TABLE_SIZE = 1024;

    size_t neigh_size = map.get_neighborhood_length();
    size_t map_size = map.get_capacity();
    size_t map_items = map.get_current_size();

    REQUIRE(neigh_size == DEFAULT_NEIGHBORHOOD_LENGTH);
    REQUIRE(map_size == 2048); /* next power of 2 greater than 1024 / load_factor*/
    REQUIRE(map_items == 0);
}

TEST_CASE("Basic Constructor with table size as parameter", "[HopscotchMap]")
{
    size_t table_size = 500;
    HopscotchMap<int, std::string> map(table_size);
    size_t map_size = map.get_capacity();
    REQUIRE(map_size == 1024);  /* next power of 2 greater than 500 / load_factor*/
}

TEST_CASE("Basic Constructor with table size and neighborhood length as parameters", "[HopscotchMap]")
{
    size_t table_size = 18;
    size_t neighborhood_length = 64;
    HopscotchMap<int, std::string> map(table_size, neighborhood_length);
    size_t map_size = map.get_capacity();
    size_t neigh_size = map.get_neighborhood_length();
    REQUIRE(map_size == 32);  /* next power of 2 greater than 18 / load_factor*/
    REQUIRE(neigh_size == 32); /* neighborhood size capped at table size */
}


/* ==================== INSERTION + FIND ==================== */

TEST_CASE("Basic insert and find", "[HopscotchMap]")
{
    HopscotchMap<int, std::string> map;

    /* Insert elements */
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

    /* Find existing elements */
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

    /* Find non-existent element */
    auto find_it4 = map.find(42);
    REQUIRE(find_it4 == map.end());
}


/* ==================== OTHER ==================== */

TEST_CASE("Iterator traversal", "[HopscotchMap]")
{
    HopscotchMap<int, int> map;
    for (int i = 0; i < 10; ++i)
    {
        map.emplace(i, i * 10);
    }

    std::vector<int> keys;
    std::vector<int> values;

    for (auto it = map.begin(); it != map.end(); ++it)
    {
        keys.push_back(it->first);
        values.push_back(it->second);
    }

    REQUIRE(keys.size() == 10);
    REQUIRE(values.size() == 10);

    for (int i = 0; i < 10; ++i)
    {
        REQUIRE(std::find(keys.begin(), keys.end(), i) != keys.end());
        REQUIRE(std::find(values.begin(), values.end(), i * 10) != values.end());
    }
}

TEST_CASE("HopscotchMap rehashing", "[HopscotchMap]")
{
    /* Small map to force rehash quickly */
    HopscotchMap<int, int> map(4, 4);

    size_t num_insertions = 10; /* more than capacity * load factor */
    for (int i = 0; i < num_insertions; ++i)
    {
        auto [it, inserted] = map.emplace(i, i * 2);
        REQUIRE(inserted);
    }

    /* All elements should be findable */
    for (int i = 0; i < num_insertions; ++i)
    {
        auto it = map.find(i);
        REQUIRE(it != map.end());
        REQUIRE(it->second == i * 2);
    }
}

/* ==================== TYPE TESTING FOR TEMPLATE ==================== */

TEST_CASE("int32 keys", "[HopscotchMap]")
{
    HopscotchMap<int32_t, std::string> map;

    auto [it1, inserted1] = map.emplace(100, "hundred");
    REQUIRE(inserted1);
    auto [it2, inserted2] = map.emplace(200, "two hundred");
    REQUIRE(inserted2);
    auto [it3, inserted3] = map.emplace(300, "three hundred");
    REQUIRE(inserted3);

    REQUIRE(map.find(100) != map.end());
    REQUIRE(map.find(200) != map.end());
    REQUIRE(map.find(300) != map.end());
    REQUIRE(map.find(400) == map.end());
}

TEST_CASE("int64 keys", "[HopscotchMap]")
{
    HopscotchMap<int64_t, std::string> map;

    auto [it1, inserted1] = map.emplace(10000000000LL, "ten billion");
    REQUIRE(inserted1);
    auto [it2, inserted2] = map.emplace(20000000000LL, "twenty billion");
    REQUIRE(inserted2);
    auto [it3, inserted3] = map.emplace(30000000000LL, "thirty billion");
    REQUIRE(inserted3);

    REQUIRE(map.find(10000000000LL) != map.end());
    REQUIRE(map.find(20000000000LL) != map.end());
    REQUIRE(map.find(30000000000LL) != map.end());
    REQUIRE(map.find(40000000000LL) == map.end());
}

TEST_CASE("Float keys", "[HopscotchMap]")
{
    HopscotchMap<float, int> map;

    auto [it1, inserted1] = map.emplace(1.5f, 10);
    REQUIRE(inserted1);
    auto [it2, inserted2] = map.emplace(2.5f, 20);
    REQUIRE(inserted2);
    auto [it3, inserted3] = map.emplace(3.5f, 30);
    REQUIRE(inserted3);

    REQUIRE(map.find(1.5f) != map.end());
    REQUIRE(map.find(2.5f) != map.end());
    REQUIRE(map.find(3.5f) != map.end());
    REQUIRE(map.find(4.5f) == map.end());
}

TEST_CASE("String keys", "[HopscotchMap]")
{
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


/* ==================== STRESS TESTS ==================== */


TEST_CASE("Mass insertion and lookup stress test", "[HopscotchMap]") {
    constexpr size_t NUM_ELEMENTS = 200000;
    HopscotchMap<int, int> map;

    /* Insert elements */
    for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
        auto [it, inserted] = map.emplace(static_cast<int>(i), static_cast<int>(i * 2));
        REQUIRE(inserted);
    }

    REQUIRE(map.get_current_size() == NUM_ELEMENTS);

    // /* Lookup random samples */
    for (size_t i = 0; i < NUM_ELEMENTS; i += 5000) {
        auto it = map.find(static_cast<int>(i));
        REQUIRE(it != map.end());
        REQUIRE(it->second == static_cast<int>(i * 2));
    }

    /* Ensure non-existing keys aren't found */
    for (size_t i = NUM_ELEMENTS; i < NUM_ELEMENTS + 100; ++i) {
        REQUIRE(map.find(static_cast<int>(i)) == map.end());
    }
}

TEST_CASE("Multiple rehashes during growth", "[HopscotchMap]") {
    HopscotchMap<int, int> map(8, 4);

    /* Insert enough to trigger multiple rehashes */
    for (int i = 0; i < 50000; ++i) {
        auto [it, inserted] = map.emplace(i, i * 3);
        REQUIRE(inserted);
    }

    REQUIRE(map.get_current_size() == 50000);

    /* Verify some data survived through rehashes */
    for (int i = 0; i < 50000; i += 7777) {
        auto it = map.find(i);
        REQUIRE(it != map.end());
        REQUIRE(it->second == i * 3);
    }
}

TEST_CASE("Iterator traversal after many insertions", "[HopscotchMap]") {
    HopscotchMap<int, int> map(64, 8);
    constexpr int ELEMENTS = 10000;

    for (int i = 0; i < ELEMENTS; ++i) {
        auto [it, inserted] = map.emplace(i, i + 100);
        REQUIRE(inserted);
    }

    /* /* Traverse and count */
    size_t count = 0;
    for (auto it = map.begin(); it != map.end(); ++it)
        ++count;

    REQUIRE(count == map.get_current_size());
}

TEST_CASE("Stress test with floating-point keys", "[HopscotchMap]") {
    HopscotchMap<float, int> map(128, 16);

    for (int i = 0; i < 10000; ++i) {
        auto [it, inserted] = map.emplace(static_cast<float>(i) * 1.5f, i);
        REQUIRE(inserted);
    }

    for (int i = 0; i < 10000; i += 1000) {
        float key = static_cast<float>(i) * 1.5f;
        auto it = map.find(key);
        REQUIRE(it != map.end());
        REQUIRE(it->second == i);
    }
}

TEST_CASE("Stress test with string keys", "[HopscotchMap]") {
    HopscotchMap<std::string, int> map(256, 16);

    constexpr int NUM_ITEMS = 10000;
    for (int i = 0; i < NUM_ITEMS; ++i) {
        auto key = "item_" + std::to_string(i);
        auto [it, inserted] = map.emplace(key, i);
        REQUIRE(inserted);
    }

    /* Lookup subset */
    for (int i = 0; i < NUM_ITEMS; i += 2500) {
        auto key = "item_" + std::to_string(i);
        auto it = map.find(key);
        REQUIRE(it != map.end());
        REQUIRE(it->second == i);
    }

    /* Count traversal */
    size_t total = 0;
    for (auto it = map.begin(); it != map.end(); ++it)
        ++total;

    REQUIRE(total == map.get_current_size());
}

TEST_CASE("Neighborhood saturation and automatic rehash", "[HopscotchMap]") {
    HopscotchMap<int, int> map(16, 4);

    /* Fill repeatedly to force neighborhood full & rehashing */
    for (int round = 0; round < 5; ++round) {
        for (int i = 0; i < 1000; ++i) {
            auto [it, inserted] = map.emplace(i + round * 1000, i);
            REQUIRE(inserted);
        }
    }

    REQUIRE(map.get_current_size() == 5000);

    /* Verify random subset */
    for (int key = 0; key < 5000; key += 123) {
        auto it = map.find(key);
        REQUIRE(it != map.end());
    }
}

TEST_CASE("Stress test with 64-bit integer keys", "[HopscotchMap]") {
    HopscotchMap<int64_t, int64_t> map(64, 8);

    constexpr int64_t N = 50000;
    for (int64_t i = 0; i < N; ++i) {
        auto [it, inserted] = map.emplace(i * 1000000LL, i);
        REQUIRE(inserted);
    }

    /* Random access checks */
    for (int64_t i = 0; i < N; i += 7000) {
        int64_t key = i * 1000000LL;
        auto it = map.find(key);
        REQUIRE(it != map.end());
        REQUIRE(it->second == i);
    }
}
