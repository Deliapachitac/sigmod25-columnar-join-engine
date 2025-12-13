## **Unchained Hashtable Implementation**
#### Kalafatsis Kyriakos sdi2200058


### **Introduction:**

The implementation of the new unchained hashtable is based on the research paper provided in the exercise description. The files that were changed/created for this part of the project are: 
* **include/unchained_hashtable.h** <-- Header file for the Hashtable
* **src/unchained_hashtable.cpp** <-- Implementation of the Hashtable functions
* **tests/unchained_hashtable_tests.cpp** <-- Unit Tests
* **src/execute.cpp** <-- Minor changes to support the new HashTable structure


### **Implementation:**

The implementation was based 100% on the paper, and follows it directly. THe structures used are:
* **Tuple:** Stores the <key, hash, value> of an entry, in order to fit in the directory slots. The hash is used so that it does not need to be recalculated.
* **Buffer Array:** Stores the tuples before the build is complete, without any order.
* **Array:** Stores the tuples after the build is complete, in prefix order.
* **Directory:** Stores the 64 bit number corresponding to the correct tuple pointer, as well as a bloom filter.
* **Tags:** Array used for the bloom filter. It contains 2048 16bit numbers, where only 4 bits are set, and is used so that any entry in the hashtable only sets 4 bits of its corresponding bloom filter

The **flow** of the hashtable is pretty simple: 
First, the vectors are added to the buffer storage. After that, the build is finalized by the user. When that happens, since we already know the tuple number, we create the directory based on that, and allocate an extra spot at the start, for the first tuple pointer. Then the next steps are: 
* **Step 1:** For each tuple in the buffer, we find the correct directory slot using its hash, update the tuple count in that slot in the first 48 bits, and the bloom filter using the tags.
* **Step 2:** For each directory slot, instead of the raw slot count, we now use the running count of all previous slots, in order to point to the correct tuple. For example, in slot 3, if the count was 12, we now add to that the count of slots 2 and 1.
* **Step 3:** For each tuple, we sort it in the correct point on the array, using the slots of the directory. At the same time, with each tuple inserted, we update the slot, in order to now show at a pointer after the given tuple.

Lastly, the probe function is used after finalizing, and it returns a vector of values, corresponding to the given key. The could_contain function helps eliminate most unwanted tuples before ever going to the tuple array, from the directory. After the correct slot was found, we traverse the array in a linear fashion, and grab all of the correct values.