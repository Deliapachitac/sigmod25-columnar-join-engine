#include <iostream>
#include <vector>
#include <iterator>
#include <climits>
#include <cstddef>  
#define FNV_offset32 ((uint32_t) 2166136261)
#define FNV_offset64 ((uint64_t) 14695981039346656037)
#define FNV_prime32  ((uint32_t) 16777619)
#define FNV_prime64  ((uint64_t) 1099511628211)


template<typename T1, typename T2> 
class rh_map{
    private:
    struct data {
        //Probe sequence length
        size_t psl;
        //Pair holding kay and value, we use pair so we can have a correct iterator
        std::pair<T1,T2> kv;
        //Flag to note that the bucket is empty
        bool is_empty = true;
    };
    static constexpr size_t DEFAULT_CAPACITY = 16; 
    static constexpr float REHASH_LOAD = 0.75;
    //Hashtable buckets, they are of type node
    std::vector<data> b;
    size_t capacity;
    size_t entries = 0;
    //We know that the possible types of data, so we hash depending on what data we have.
    uint32_t FNV1a_32(const int32_t key){
        uint32_t hash = FNV_offset32;
        for(size_t i = 0; i < 4; i++){
            uint8_t byte = (key >> (i*8)) & 0xFF;
            hash ^= byte;
            hash *= FNV_prime32;
        }
        return hash;
    }
    uint64_t FNV1a_64(const int64_t key){
        uint64_t hash = FNV_offset64;
        for(size_t i = 0; i < 8; i++){
            uint8_t byte = (key >> (i*8)) & 0xFF;
            hash ^= byte;
            hash *= FNV_prime64;
        }
        return hash;
    }
    uint64_t FNV1a_str(const std::string& key){
        uint64_t hash = FNV_offset64;
        for(char byte: key){
            hash^=static_cast<uint8_t>(byte);
            hash*=FNV_prime64;
        }
        return hash;
    }
    int32_t HashFunction(T1 key) {
        if constexpr (std::is_same_v<T1, int32_t>) {
            return FNV1a_32(key) % capacity;
        } else if constexpr (std::is_same_v<T1, int64_t>) {
            return FNV1a_64(key) % capacity;
        } else if constexpr (std::is_same_v<T1, double>) {
            return FNV1a_64(*reinterpret_cast<int64_t*>(&key)) % capacity;
        }else if constexpr(std::is_same_v<T1,std::string>){
            return FNV1a_str(key) % capacity;
        }
    }
    //TODO: Implement rehash
    void Rehash(){
        return;
    }
    public:
    struct Iterator{
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = std::pair<T1,T2>;
        using pointer = value_type*;
        using reference = value_type&;
        
        rh_map* table;
        size_t index;

        Iterator(rh_map* tbl, size_t idx) : table(tbl), index(idx) {
            skip_empty();
        }

        void skip_empty() {
            while (index < table->b.size() && table->b[index].is_empty) 
                ++index;
        }
        
        value_type& operator*() const { return table->b[index].kv; }
        value_type* operator->() const { return &table->b[index].kv; }

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
            return a.table == b.table && a.index == b.index;
        }
        friend bool operator!=(const Iterator& a, const Iterator& b) {
            return !(a == b);
        }
    };
    public:
    //If size is given, then
    rh_map(size_t size) : b(static_cast<size_t>(size/REHASH_LOAD)+1), capacity(static_cast<size_t>(size/REHASH_LOAD)+1){}
    rh_map() : b(DEFAULT_CAPACITY), capacity(DEFAULT_CAPACITY){}
    Iterator begin() {
        return Iterator(this,0);
    }
    Iterator end(){
        return Iterator(this,b.size());
    }
    std::pair<Iterator , bool> emplace(const T1& key, const T2& value){
        return this->insert({key, value});
    }
    //Inserts a key to the hashmap
    //Return a pair, the first value is an iterator where we inserted the key, or one past the end if the key was already in the map,
    //and a bool that signals if the operation was successful
    std::pair<Iterator , bool> insert(const std::pair<T1,T2>& values){
        int32_t inserted_index;
        std::pair<T1,T2> kv = values;
        float load_factor = float(entries)/capacity;
        if(load_factor >= this->REHASH_LOAD){
            this->Rehash();
        }
        auto index = this->HashFunction(kv.first);
        //If the index bucket is empty insert there, otherwise insert elsewhere
        if(this->b[index].is_empty){
            this->entries++;
            this->b[index].is_empty = false;
            this->b[index].psl = 0;
            this->b[index].kv = kv;
            return { Iterator(this,index) , true };
        }
        //This means we have a colission
        bool flag = true;
        size_t psl = 1;
        while(true){
            //Hashmap wraps around when at the end of the vector
            index = (index + 1) % capacity;
            if(this->b[index].is_empty){
                if(flag){
                    inserted_index = index;
                    flag = false;
                }
                this->entries++;
                this->b[index].is_empty = false;
                this->b[index].psl = psl;
                this->b[index].kv = kv;
                break;
            }
            //Check if key is already in map
            else if(/*key*/ this->b[index].kv.first == kv.first){
                return { Iterator(this,index), false };
            }
            else if(this->b[index].psl < psl){
                //We may go through this step multiple times, we make sure we return the right index
                if(flag){
                    inserted_index = index;
                    flag = false;
                }
                std::swap(this->b[index].kv, kv);
                std::swap(this->b[index].psl, psl);
            }
            psl++;
        }
        return {Iterator(this,inserted_index), true};
        
    }
    //Searches for the value with the given key, returns iterator at the desired value or iterator one past the last index
    Iterator find(const T1& key){
        int32_t index = this->HashFunction(key);
        size_t start = index;
        size_t psl = 0;
        do {
            if(this->b[index].kv.first == key)
                return Iterator(this,index);

            //If empty slot is found before finding the key, this means the key is not in the map
            if(this->b[index].is_empty)
                break;
            // early stop
            if (this->b[index].psl < psl) 
                break;  

            //Wrap around the vector
            index = (index + 1) % capacity;
            psl++;
        }while (index != start);
        return Iterator(this, b.size()); 
    }
    bool empty() const{
        return this->entries == 0;
    }
    size_t size() const{
        return this->entries;
    }
    size_t get_capacity() const {
        return this->capacity;
    }   
    //Helper functions for unit tests, they don't search with keys but with index
    //TODO: Make them return unique if bucket is empty
    bool get_is_empty(int32_t i) const{
        return this->b[i].is_empty;
    }
    size_t get_psl(int32_t i) const{
        if(this->b[i].is_empty){
            return std::numeric_limits<signed_t>::max();
        }
        return this->b[i].psl;
    }
    T2 get_v(int32_t i) const{
        return this->b[i].kv.second;
    }
};