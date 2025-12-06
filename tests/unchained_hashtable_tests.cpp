#include <catch2/catch_test_macros.hpp>
#include "unchained_hashtable.h"

/* ============================================================
   CONSTRUCTION
============================================================ */

TEST_CASE("UnchainedHT basic construction", "[unchained_ht]") {
    unchained_ht ht(128);


    ht.finalize_build();
    // table_size is rounded to next power of 2
    // 128 is already power of 2
    REQUIRE(ht.lookup(0).first == nullptr); // not built yet, but lookup throws
}

TEST_CASE("UnchainedHT next power of two behavior", "[unchained_ht]") {
    unchained_ht ht(300); // should round to 512

    

    // Can't directly access table_size (private),
    // but check build_insert + finalize + lookup basic behavior.
    ht.build_insert(1, 11);
    ht.finalize_build();

    auto [s,e] = ht.lookup(1);
    REQUIRE(s != nullptr);
    REQUIRE(e == s + 1);
    REQUIRE(s->value == 11);
}

/* ============================================================
   INSERT + FINALIZE + LOOKUP
============================================================ */

TEST_CASE("UnchainedHT simple insert and lookup", "[unchained_ht]") {
    unchained_ht ht(64);

    ht.build_insert(1, 100);
    ht.build_insert(2, 200);
    ht.build_insert(3, 300);

    ht.finalize_build();

    auto [s1,e1] = ht.lookup(1);
    REQUIRE(s1 != nullptr);
    REQUIRE(e1 == s1 + 1);
    REQUIRE(s1->value == 100);

    auto [s2,e2] = ht.lookup(2);
    REQUIRE(s2 != nullptr);
    REQUIRE(e2 == s2 + 1);
    REQUIRE(s2->value == 200);

    auto [s3,e3] = ht.lookup(3);
    REQUIRE(s3 != nullptr);
    REQUIRE(e3 == s3 + 1);
    REQUIRE(s3->value == 300);

    auto [s4,e4] = ht.lookup(42);
    REQUIRE(s4 == nullptr);
    REQUIRE(e4 == nullptr);
}

TEST_CASE("UnchainedHT duplicate key behavior", "[unchained_ht]") {
    unchained_ht ht(64);

    ht.build_insert(5, 10);
    ht.build_insert(5, 20);
    ht.build_insert(5, 30);

    ht.finalize_build();

    auto [s,e] = ht.lookup(5);
    REQUIRE(s != nullptr);
    REQUIRE(e != nullptr);
    REQUIRE(e - s == 3);

    REQUIRE(s[0].value == 10);
    REQUIRE(s[1].value == 20);
    REQUIRE(s[2].value == 30);
}

/* ============================================================
   FINALIZATION / ERROR HANDLING
============================================================ */

TEST_CASE("UnchainedHT lookup before finalize throws", "[unchained_ht]") {
    unchained_ht ht(32);
    ht.build_insert(1, 10);

    REQUIRE_THROWS(ht.lookup(1));
}

TEST_CASE("UnchainedHT double finalize is harmless", "[unchained_ht]") {
    unchained_ht ht(32);

    ht.build_insert(1, 99);
    ht.finalize_build();
    REQUIRE_NOTHROW(ht.finalize_build());

    auto [s,e] = ht.lookup(1);
    REQUIRE(s != nullptr);
    REQUIRE(s->value == 99);
}

TEST_CASE("UnchainedHT insert after finalize throws", "[unchained_ht]") {
    unchained_ht ht(32);

    ht.build_insert(10, 100);
    ht.finalize_build();

    REQUIRE_THROWS(ht.build_insert(20, 200));
}

/* ============================================================
   BLOOM FILTER / LOAD STRESS
============================================================ */

TEST_CASE("UnchainedHT medium stress test", "[unchained_ht]") {
    constexpr size_t N = 50000;
    unchained_ht ht(N);

    for (size_t i = 0; i < N; ++i)
        ht.build_insert((int32_t)i, i * 3);

    ht.finalize_build();

    for (size_t i = 0; i < N; i += 7777) {
        auto [s,e] = ht.lookup((int32_t)i);
        REQUIRE(s != nullptr);
        REQUIRE(s->value == i * 3);
    }

    auto [s2,e2] = ht.lookup((int32_t)(N + 12345));
    REQUIRE(s2 == nullptr);
}

TEST_CASE("UnchainedHT large contiguous duplicate keys", "[unchained_ht]") {
    unchained_ht ht(4096);

    for (int i = 0; i < 10000; ++i)
        ht.build_insert(777, i);

    ht.finalize_build();

    auto [s,e] = ht.lookup(777);
    REQUIRE(s != nullptr);
    REQUIRE(e != nullptr);
    REQUIRE(e - s == 10000);

    REQUIRE(s[0].value == 0);
    REQUIRE(s[9999].value == 9999);
}

/* ============================================================
   RANDOMIZED INSERT/LOOKUP STRESS
============================================================ */

TEST_CASE("UnchainedHT random stress test", "[unchained_ht]") {
    constexpr size_t N = 80000;

    std::vector<int32_t> keys;
    keys.reserve(N);

    for (size_t i = 0; i < N; ++i)
        keys.push_back(rand());

    unchained_ht ht(N);

    for (size_t i = 0; i < N; ++i)
        ht.build_insert(keys[i], i);

    ht.finalize_build();

    for (size_t i = 0; i < N; i += 5000) {
        auto [s,e] = ht.lookup(keys[i]);
        REQUIRE(s != nullptr);
        REQUIRE(s->value == i);
    }
}

