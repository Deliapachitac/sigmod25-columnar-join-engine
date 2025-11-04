## Project Part 1
Cuckoo Hashing

Name: Delia-Maria
Surname: Pachitac
ID: 1115202200125

# Implementation Details
The implementation of cuckoo hashing contains a struct Data:
- kv: a pair that contains the key and the value.
- occupied: a variable indicating whether the bucket is occupied.

The buckets are stored in two vectors, T1 and T2 (two arrays with the two different capacities).
I choose to change the shared capacity into two dofferent ones for each table because th rehash was triggered based on the combined load factor of both tables. For example :
Imagine the following situation:
    -T1 has 16 elements and reaches 100% of its capacity
    -T2 is  empty having 0 elements 
In the old design the total load factory for both tables would be at 50% and we would double the size of both tables and reinsertall the elements . But in the T2 is still has plenty of free space and we may  pay an expensive cost for doubling T2. 


# Hashing
For hashing, we have two functions: h1 and h2.
These functions are based on the FNV-1a algorithm for different key types (int32, int64, double, string).
The table size is a power of two, allowing the use of bitmasking instead of modulo, which increases speed.

# Emplace
1. Check before inserting each table separetly if they have reached the load factory and rehash 

2. Compute h1 and h2 for the key

3. Try inserting (key, value) in T1[h1(key)]      
    - If empty -> place it there        
    - If occupied -> check for duplicates

4. If it is a duplicate  , else  swap the existing element and try to move it to the second table based on the h2

5. Continue swapping until either an empty spot is found or a cycle is detected.

6. If a cycle occurs -> rehash the table (double capacity for both tables and reinsert all elements).

# Find
The find function checks the two possible positions:
T1[h1(key)] or T2[h2(key)].
If the key is found, it returns an iterator to that position; otherwise, it returns end().

# Test Coverage Summary
- Constructors  (Default constructorand size constructor)
- Insertion with collisions
- Insertion with cycle Detection and rehashing
- Duplicate Key Handling
- Find Function
- Clear Function
- Iterators
- Multiple Key Types (int32, int64, double, std::string)

# Complexity 
After implementing and executing the 3 hashing algorithms, we realized that in the cuckoo hash when a  cycle occurs or the tables have reached the load factor, several swaps plus possible rehashing are required. This extra movement makes insertions slower compared to algorithms like robin hood or hopscotch. Also the cuckoo hashing accesses two separate tables, which requires more memory access and  it slows down the program. On the other side the other two algorithms operate in a single contiguous array making them faster.
