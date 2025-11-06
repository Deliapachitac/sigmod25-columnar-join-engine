#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <functional>
#include <limits>
#include <climits>
#include <cstddef>  
#include <cmath>

#define FNV_offset32 ((uint32_t) 2166136261U)
#define FNV_offset64 ((uint64_t) 14695981039346656037ULL)
#define FNV_prime32  ((uint32_t) 16777619U)
#define FNV_prime64  ((uint64_t) 1099511628211ULL)
#define MAX_KICKS 500
#define TABLE_ONE 1
#define TABLE_TWO 2
template<typename K, typename V>
class cuckoo_map {
    private:

        // Entry structure for each bucket
        struct Data {
            std::pair<K, V> kv;   // key-value pair
            bool occupied = false;
        };

        // Two hash tables (both have same capacity)
        std::vector<Data> T1, T2;

        // Variables to keep track of capacity, size and load factor
        size_t capacity1, capacity2;    // each table has its own capacity
        size_t entries1, entries2;         // total number of entries in each table    
        static constexpr double LOAD_FACTOR = 0.5;
        static constexpr size_t DEFAULT_CAPACITY = 16;  

        //We know that the possible types of data, so we hash depending on what data we have.
        uint32_t FNV1a_32(const int32_t key) const {
            uint32_t hash = FNV_offset32;
            for(size_t i = 0; i < 4; i++){
                uint8_t byte = (key >> (i*8)) & 0xFF;
                hash ^= byte;
                hash *= FNV_prime32;
            }
            return hash;
        }

        uint64_t FNV1a_64(const int64_t key) const {
            uint64_t hash = FNV_offset64;
            for(size_t i = 0; i < 8; i++){
                uint8_t byte = (key >> (i*8)) & 0xFF;
                hash ^= byte;
                hash *= FNV_prime64;
            }
            return hash;
        }

        uint64_t FNV1a_str(const std::string& key) const {
            uint64_t hash = FNV_offset64;
            for(char byte: key){
                hash ^= static_cast<uint8_t>(byte);
                hash *= FNV_prime64;
            }
            return hash;
        }

        // use K by-value (copy) as parameter
        size_t h1(K key) const {
            size_t capacity = capacity1; 
            if constexpr (std::is_same_v<K, int32_t> || std::is_same_v<K, int>) {
                return FNV1a_32(static_cast<int32_t>(key)) & (capacity-1);
            } else if constexpr (std::is_same_v<K, uint32_t>) {
                return FNV1a_32(static_cast<int32_t>(key)) & (capacity-1);
            } else if constexpr (std::is_same_v<K, int64_t>) {
                return FNV1a_64(key) & (capacity-1);
            } else if constexpr (std::is_same_v<K, uint64_t>) {
                return FNV1a_64(static_cast<int64_t>(key)) & (capacity-1);
            } else if constexpr (std::is_same_v<K, double>) {
                int64_t bits;
                std::memcpy(&bits, &key, sizeof(double));
                return FNV1a_64(bits) & (capacity-1);
            } else if constexpr(std::is_same_v<K,std::string>){
                return FNV1a_str(key) & (capacity-1);
            }
        }

        size_t h2(K key) const {
            size_t capacity = capacity2;
            if constexpr (std::is_same_v<K, int32_t> || std::is_same_v<K, int>) {
                return (FNV1a_32(static_cast<int32_t>(key)) * 0x27d4eb2dU + 0x85ebca6bU) & (capacity - 1);
            } else if constexpr (std::is_same_v<K, uint32_t>) {
                return (FNV1a_32(static_cast<int32_t>(key)) * 0x27d4eb2dU + 0x85ebca6bU) & (capacity - 1);
            } else if constexpr (std::is_same_v<K, int64_t> || std::is_same_v<K, uint64_t>) {
                uint64_t val = (std::is_same_v<K, uint64_t>) ? static_cast<int64_t>(key) : key;
                uint64_t h = FNV1a_64(val);
                h ^= h >> 33;
                h *= 0xff51afd7ed558ccdULL;
                return h & (capacity - 1);
            } else if constexpr (std::is_same_v<K, double>) {
                int64_t bits;
                std::memcpy(&bits, &key, sizeof(double));  // safe bitwise copy
                uint64_t h = FNV1a_64(bits);
                h ^= h >> 33;
                h *= 0xff51afd7ed558ccdULL;
                return h & (capacity - 1);
            } else if constexpr (std::is_same_v<K, std::string>) {
                uint64_t h = FNV1a_str(key);
                h ^= h >> 32;
                h *= 0x9e3779b97f4a7c15ULL;
                return h & (capacity - 1);
            }
        }

        size_t closestPowerOfTwo(size_t n) {
            if (n == 0) return 1;
            n--;
            n |= n >> 1;
            n |= n >> 2;
            n |= n >> 4;
            n |= n >> 8;
            n |= n >> 16;
            if constexpr (sizeof(size_t) == 8) // 64-bit
                n |= n >> 32;
            return n + 1;
        }


        // Rehash: double capacity and reinsert everything for one table 
        void rehash_one_table(int table_number) {
            if (table_number == TABLE_ONE) {
                capacity1 *= 2;
                std::vector<Data> oldT1 = std::move(T1);
                T1 = std::vector<Data>(capacity1);
                entries1 = 0;

                for (const auto& e : oldT1) {
                    if (e.occupied)
                        insert(e.kv.first, e.kv.second);
                }
            } 
            else if (table_number == TABLE_TWO) {
                capacity2 *= 2;
                std::vector<Data> oldT2 = std::move(T2);
                T2 = std::vector<Data>(capacity2);
                entries2 = 0;

                for (const auto& e : oldT2) {
                    if (e.occupied)
                        insert(e.kv.first, e.kv.second);
                }
            } 
            else {
                throw std::invalid_argument("Invalid table number. Must be 1 or 2.");
            }
        }

        void rehash_both() {
            capacity1 *= 2;
            capacity2 *= 2;
            std::vector<Data> oldT1 = std::move(T1);
            std::vector<Data> oldT2 = std::move(T2);
            T1 = std::vector<Data>(capacity1);
            T2 = std::vector<Data>(capacity2);
            entries1 = entries2 = 0;
            for (const auto& e : oldT1)
                if (e.occupied) insert(e.kv.first, e.kv.second);
            for (const auto& e : oldT2)
                if (e.occupied) insert(e.kv.first, e.kv.second);
        }

    public:

        // Iterator for non-const access
        struct Iterator {
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = std::pair<K,V>;
            using pointer = value_type*;
            using reference = value_type&;

            cuckoo_map* table;
            size_t index;
            int current_table; // 1 for T1, 2 for T2

            Iterator(cuckoo_map* tbl, size_t idx = 0, int tbl_num = TABLE_ONE)
                : table(tbl), index(idx), current_table(tbl_num) {
                skip_empty();
            }

            void skip_empty() {
                while (current_table <= TABLE_TWO) {
                    auto& t = (current_table == TABLE_ONE) ? table->T1 : table->T2;
                    while (index < t.size() && !t[index].occupied)
                        ++index;
                    if (index < t.size()) return;
                    current_table++;
                    index = 0;
                }
            }

            reference operator*() const {
                return (current_table == TABLE_ONE) ? table->T1[index].kv : table->T2[index].kv;
            }

            pointer operator->() const {
                return &(**this);
            }

            Iterator& operator++() {
                ++index;
                skip_empty();
                return *this;
            }

            Iterator operator++(int) {
                Iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            friend bool operator==(const Iterator& a, const Iterator& b) {
                return a.table == b.table && a.index == b.index && a.current_table == b.current_table;
            }

            friend bool operator!=(const Iterator& a, const Iterator& b) {
                return !(a == b);
            }
        };

        Iterator begin() { return Iterator(this); }
        Iterator end() { return Iterator(this, 0, 3); }// table=3 indicates past-the-end

        // Constructors
        cuckoo_map(size_t size): T1(this->closestPowerOfTwo(static_cast<size_t>(size/LOAD_FACTOR)+1)), T2(this->closestPowerOfTwo(static_cast<size_t>(size/LOAD_FACTOR)+1)), capacity1(this->closestPowerOfTwo(static_cast<size_t>(size/LOAD_FACTOR)+1)), capacity2(this->closestPowerOfTwo(static_cast<size_t>(size/LOAD_FACTOR)+1)), entries1(0), entries2(0) {}
        cuckoo_map(): T1(DEFAULT_CAPACITY), T2(DEFAULT_CAPACITY),capacity1(DEFAULT_CAPACITY), capacity2(DEFAULT_CAPACITY), entries1(0), entries2(0) {}

        std::pair<Iterator, bool> emplace(const K& key, const V& value) {
            return this->insert(key, value) ;
        }
        // Insert operation
        //Returns true if insertion was successful
        //If the key already exists we do not insert and return false
        std::pair<Iterator, bool> insert(const K& key, const V& value) {

            // Check if we have to rehash before insertion based on load factor
            // Check per-table load
            if (double(entries1) / capacity1 >= LOAD_FACTOR)
                rehash_one_table(TABLE_ONE);
            if (double(entries2) / capacity2 >= LOAD_FACTOR)
                rehash_one_table(TABLE_TWO);

            Data new_entry{{key, value}, true};
            while (true) {
                size_t total_changes = 0;
                size_t max_entries = std::min<size_t>(std::max<size_t>(entries1 + entries2, 1), MAX_KICKS);
                for (; total_changes < max_entries; ++total_changes) {
                    auto pos1 = h1(new_entry.kv.first);

                    // If empty, insert directly
                    if (!T1[pos1].occupied) {
                        T1[pos1] = new_entry;
                        entries1++;
                        return { Iterator(this, pos1, 1), true };
                    }

                    // Duplicate key in T1
                    if (T1[pos1].kv.first == new_entry.kv.first) {
                        return { Iterator(this, pos1, 1), false };
                    }

                    std::swap(new_entry, T1[pos1]);

                    auto pos2 = h2(new_entry.kv.first);

                    // If empty, insert into T2
                    if (!T2[pos2].occupied) {
                        T2[pos2] = new_entry;
                        entries2++;
                        return { Iterator(this, pos2, 2), true };
                    }

                    // Duplicate key in T2
                    if (T2[pos2].kv.first == new_entry.kv.first) {
                        return { Iterator(this, pos2, 2), false };
                    }

                    std::swap(new_entry, T2[pos2]);
                }

                // Cycle detected, rehash and try again
                rehash_both();
            }

            // Should never reach here
            return { Iterator(this, 0, 3), false };
        }


        Iterator find(const K& key) {
            if (capacity1 == 0 && capacity2 == 0)
                return Iterator(this, 0, 3); // end()

            size_t p1 = h1(key);
            if ( T1[p1].occupied && T1[p1].kv.first == key) {
                return Iterator(this, p1, 1);
            }

            size_t p2 = h2(key);
            if (T2[p2].occupied && T2[p2].kv.first == key) {
                return Iterator(this, p2, 2);
            }

            return Iterator(this, 0, 3); // end()
        }


        // Functions to get current size , capacity  and check if both tables are empty
        size_t get_capacity(int table_number) const { 
            return (table_number == TABLE_ONE) ? capacity1 : capacity2;
        }
        size_t size() const { 
            return entries1 + entries2; 
        }
        bool empty() const { 
            return (entries1 + entries2) == 0; 
        }

        // Functions that check if a bucket is empty in table T1 and T2
        bool get_is_empty_table1(int32_t i) const {
            if (i < 0 || static_cast<size_t>(i) >= T1.size()) return true;
            return !T1[static_cast<size_t>(i)].occupied;
        }
        bool get_is_empty_table2(int32_t i) const {
            if (i < 0 || static_cast<size_t>(i) >= T2.size()) return true;
            return !T2[static_cast<size_t>(i)].occupied;
        }

        // General  function to check if a bucket is empty in either table using the previous functions
        bool get_is_empty(int table, int32_t i) const {
            return (table == TABLE_ONE) ? get_is_empty_table1(i) : get_is_empty_table2(i);
        }

        void clear() {
            for (auto& e : T1)
                e.occupied = false;
            for (auto& e : T2)
                e.occupied = false;
            entries1 = 0;
            entries2 = 0;
        }

        V get_value (int table, int32_t i) const {
            if (table == TABLE_ONE) {
                if (i < 0 || static_cast<size_t>(i) >= T1.size() || !T1[static_cast<size_t>(i)].occupied) {
                    throw std::out_of_range("Invalid index or empty bucket in T1");
                }
                return T1[static_cast<size_t>(i)].kv.second;
            } else {
                if (i < 0 || static_cast<size_t>(i) >= T2.size() || !T2[static_cast<size_t>(i)].occupied) {
                    throw std::out_of_range("Invalid index or empty bucket in T2");
                }
                return T2[static_cast<size_t>(i)].kv.second;
            }
        }

        //print the contents of the hash tables
        void print() const {
            std::cout << "T1:\n";
            for (size_t i = 0; i < capacity1; ++i) {
                if (T1[i].occupied)
                    std::cout << "[" << i << "] " << T1[i].kv.first
                            << " -> " << T1[i].kv.second << "\n";
            }

            std::cout << "T2:\n";
            for (size_t i = 0; i < capacity2; ++i) {
                if (T2[i].occupied)
                    std::cout << "[" << i << "] " << T2[i].kv.first
                            << " -> " << T2[i].kv.second << "\n";
            }
        }
};

