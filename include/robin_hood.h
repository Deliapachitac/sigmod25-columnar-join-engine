#include <iostream>
#include <vector>
#define FNV_offset32 ((uint32_t) 2166136261)
#define FNV_offset64 ((uint64_t) 14695981039346656037)
#define FNV_prime32  ((uint32_t) 16777619)
#define FNV_prime64  ((uint64_t) 1099511628211)
template<typename T> 
struct data {
    //Probe sequence length
    size_t PSL;
    // Value is of type T
    T v; 
    //Flag to note that the bucket is empty
    bool is_empty = true;
};

template<typename T1, typename T2> 
class RobinHoodHash{

    private:

    static constexpr size_t DEFAULT_CAPACITY = 16; 
    static constexpr float REHASH_LOAD = 0.75;
    //Hashtable buckets, they are of type node
    std::vector<data<T2>> b;
    size_t capacity;
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
    uint64_t FNV1a_str(const std::string key){
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

    public:

    RobinHoodHash(size_t size) : b(size), capacity(size){}
    RobinHoodHash() : b(DEFAULT_CAPACITY), capacity(DEFAULT_CAPACITY){}

};