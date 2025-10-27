#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <functional>
#include <limits>

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
        size_t capacity;         // both tables have the same capacity
        size_t entries;          // total number of entries in both tables
        static constexpr double LOAD_FACTOR = 0.5;
        static constexpr size_t DEFAULT_CAPACITY = 16;  

        // Easy hash function 
        // CHANGE IT 
        size_t h1(const K& key) const {
            return std::hash<K>{}(key) % capacity;
        }

        size_t h2(const K& key) const {
            return (std::hash<K>{}(key) / capacity) % capacity;
        }

        

        // Rehash: double capacity and reinsert everything
        // Classic approach (you can change it with incremental rehashing)
        void rehash() {
            
            capacity *= 2;

            std::vector<Data> oldT1 = std::move(T1);
            std::vector<Data> oldT2 = std::move(T2);

            T1 = std::vector<Data>(capacity);
            T2 = std::vector<Data>(capacity);
            entries = 0;

            for (const auto& e : oldT1)
                if (e.occupied)
                    insert(e.kv.first, e.kv.second);

            for (const auto& e : oldT2)
                if (e.occupied)
                    insert(e.kv.first, e.kv.second);
        }

    public:

        // Constructors
        cuckoo_map(size_t size): T1(static_cast<size_t>(size/LOAD_FACTOR)+1), T2(static_cast<size_t>(size/LOAD_FACTOR)+1), capacity((static_cast<size_t>(size/LOAD_FACTOR)+1)), entries(0) {}
        cuckoo_map(): T1(DEFAULT_CAPACITY), T2(DEFAULT_CAPACITY),capacity(DEFAULT_CAPACITY), entries(0) {}

        // Insert operation
        //Returns true if insertion was successful
        //If the key already exists we do not insert and return false
        bool insert(const K& key, const V& value) {

            // Check if  we have to rehash before insertion based on load factor
            double current_load = static_cast<double>(entries) / (2.0 * capacity);
            if (current_load > LOAD_FACTOR) {
                std::cout << "[Rehash triggered] Load factor = " << current_load << " > " << LOAD_FACTOR << "\n";
                rehash();
            }

            // This loop is necessary because it  handles multiple rehashes in case of cycles
            while (true) {

                // Create the new entry that we want to insert
                Data new_entry{{key, value}, true};
                
                // Variables that help with cycle detection
                // total_changes :  counts how many displacements we have done from one table to the other
                // max_entries   : if the entries is 0 (first insertion) we set it to 1 to avoid infinite loop
                size_t total_changes = 0; 
                size_t max_entries = std::max<size_t>(entries, 1);  

                //If we have done more displacements than the number of entries then we have a cycle and need to rehash
                for (; total_changes < max_entries; ++total_changes) {
                    size_t pos1 = h1(new_entry.kv.first);

                    // If position in the first hash table(T1) is empty then insert 
                    if (!T1[pos1].occupied) {
                        T1[pos1] = new_entry;
                        entries++;
                        return true;
                    }

                    // Checking for duplicate key if the position is not empty
                    if ( T1[pos1].kv.first  == new_entry.kv.first) {
                        return   false;     
                    }
                            
                    // Swap the existing entry with the new one
                    std::swap( new_entry,  T1[pos1]);

                    //Now calculate the postion in the second table using another hash function
                    size_t  pos2 = h2(  new_entry.kv.first );

                    // If position in the  second hash table(T2) is empty then insert
                    if (!T2[pos2].occupied) {
                        T2[pos2] = new_entry;
                        entries++;
                        return true;
                    }

                    // Check for duplicate key in the second table(T2)
                    if (T2[pos2].kv.first == new_entry.kv.first) {
                        return false; 
                    }

                    // Swap again the existing entry with the previously swapped entry and repeatagain the proccess  
                    std::swap(new_entry, T2[pos2]);
                }

                // If we got here the total changes exceeded the number of  entries so we have  a cycle
                std::cerr << "Cycle detected " << new_entry.kv.first;
                rehash();

                // Try the insertion again after rehash 
            }

            // We Should never reach  here but just in case 
            return false;
        }

        // Find operation 
        // In the out_value parameter we store the found value if the key exists
        bool find(const K& key, V& out_value)  {
            if (capacity == 0) return false; 
            size_t p1 = h1(key);
            if (p1 < T1.size() && T1[p1].occupied && T1[p1].kv.first == key) {
                out_value = T1[p1].kv.second;
                return true;
            }
            size_t p2 = h2(key);
            if (p2 < T2.size() && T2[p2].occupied && T2[p2].kv.first == key) {
                out_value = T2[p2].kv.second;
                return true;
            }
            return false;
        }


        // Functions to get current size , capacity  and check if both tables are empty
        size_t get_capacity() const { 
            return capacity; 
        }
        size_t size() const { 
            return entries; 
        }
        bool empty() const { 
            return entries == 0; 
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
            return (table == 1) ? get_is_empty_table1(i) : get_is_empty_table2(i);
        }

        //print the contents of the hash tables
        void print() const {
            std::cout << "T1:\n";
            for (size_t i = 0; i < capacity; ++i) {
                if (T1[i].occupied)
                    std::cout << "[" << i << "] " << T1[i].kv.first
                            << " -> " << T1[i].kv.second << "\n";
            }

            std::cout << "T2:\n";
            for (size_t i = 0; i < capacity; ++i) {
                if (T2[i].occupied)
                    std::cout << "[" << i << "] " << T2[i].kv.first
                            << " -> " << T2[i].kv.second << "\n";
            }
        }
};
