#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <functional>
#include <limits>

template<typename K, typename V>

class cuckoo_map {
    private:

        // Entry  structure for each bucket 
        struct data {
            std::pair<K, V> key_value;
            bool occupied = false;
        };

        // Two hash tables and their type is data struct
        std::vector<data> H1, H2;

        // Variables to keep track of capacity ,size and load factor
        size_t capacity;
        size_t entries;
        const double LOAD_FACTOR = 0.5;
        const size_t DEFAULT_CAPACITY = 16; // CHANGE IT ONLY FOR TESTING

        // Easy hash function 
        // CHANGE IT 
        size_t h1(const K key) {
            return std::hash<K>{}(key) % capacity;
        }

        size_t h2(const K key)  {
            return (std::hash<K>{}(key ^ 0x9e3779b9)) % capacity;
        }

        // Rehash: double capacity and reinsert everything
        // Classic way maybe CHANGE IT with incremental rehashing PIAZZA 
        void rehash() {
            size_t old_capacity = capacity;
            capacity *= 2;
            std::vector<data> oldH1 = std::move(H1);
            std::vector<data> oldH2 = std::move(H2);

            H1 = std::vector<data>(capacity);
            H2 = std::vector<data>(capacity);
            entries = 0;

            for (const auto& e : oldH1)
                if (e.occupied)
                    insert(e.key_value.first, e.key_value.second);

            for (const auto& e : oldH2)
                if (e.occupied)
                    insert(e.key_value.first, e.key_value.second);
        }

    public:
        cuckoo_map(size_t init_capacity = DEFAULT_CAPACITY)
            : capacity(init_capacity), entries(0), LOAD_FACTOR(0.5), DEFAULT_CAPACITY(16) {
            H1.resize(capacity);
            H2.resize(capacity);
        }

        bool insert(std::pair<K, V>& key_value) {
            
        }

        bool find(std::pair<K, V>& key_value) const {
            
        }

        bool erase(const K& key) {
            
        }

        size_t size() const { return entries; }
        size_t get_capacity() const { return capacity * 2; }

        void print() const {
            std::cout << "T1:\n";
            for (size_t i = 0; i < capacity; ++i) {
                if (T1[i].occupied)
                    std::cout << "[" << i << "] (" << T1[i].key << ", " << T1[i].value << ")\n";
            }
            std::cout << "T2:\n";
            for (size_t i = 0; i < capacity; ++i) {
                if (T2[i].occupied)
                    std::cout << "[" << i << "] (" << T2[i].key << ", " << T2[i].value << ")\n";
            }
        }

};