#include <table.h>
#include <thread>
#include <cstdint>

// 64-bit packed value: [table_idx(16) | column_idx(16) |page_idx(16) |data_idx(16)]
// If data_idx is 0xFFFF then entry is null        
// If data_idx is 0xFFFE then entry is int and table_idx is the 16 lower bits of the int entry and column_idx is the 16 higher order bits     
struct value_t{
    uint64_t packed;
    
    value_t() : packed(0) {}
    value_t(uint16_t table, uint16_t column, uint16_t page, uint16_t data) 
        : packed(((uint64_t)table) | ((uint64_t)column << 16) | ((uint64_t)page << 32) | ((uint64_t)data << 48)) {}
    
    uint16_t table_idx() const { return (uint16_t)(packed & 0xFFFF); }
    uint16_t column_idx() const { return (uint16_t)((packed >> 16) & 0xFFFF); }
    uint16_t page_idx() const { return (uint16_t)((packed >> 32) & 0xFFFF); }
    uint16_t data_idx() const { return (uint16_t)((packed >> 48) & 0xFFFF); }
    
    void set_table_idx(uint16_t val) { packed = (packed & ~0xFFFFULL) | val; }
    void set_column_idx(uint16_t val) { packed= (packed & ~0xFFFF0000ULL)|((uint64_t)val << 16); }
    void set_page_idx(uint16_t val) { packed =(packed & ~0xFFFF00000000ULL) | ((uint64_t)val << 32);}
    void set_data_idx(uint16_t val) { packed = (packed & ~0xFFFF000000000000ULL) | ((uint64_t)val << 48); }
};

std::vector<std::vector<value_t>> scan_table(const ColumnarTable& ,
     const std::vector<std::tuple<size_t, DataType>>& , const size_t& );

std::vector<std::vector<Data>> materialize_table(const std::vector<std::vector<value_t>>& , const Plan& );

ColumnarTable materialize_columnar_table(const std::vector<std::vector<value_t>>& , 
                                        const Plan& , 
                                        const std::vector<DataType>& );
       
///////////////////////////////////////////////////////////////
/////////// BEGINNING OF COLUMNAR STORAGE CODE ////////////////
////////////////////////EXERCISE 2 ////////////////////////////
///////////////////////////////////////////////////////////////
                                        
// Structs for the second part of the project (columnar storage)  
struct column_t {
    size_t num_rows;  // number of rows in the table 
    DataType  type; 
    std::vector<std::vector<Page*>> threadPages;
    std::vector<Page*> pages;
    bool valid;  // true if pages were allocated and should be deleted             
                 // false ifpages were moved and  should not be deleted 
    bool collected = false;      
    
    column_t(DataType dtype) : threadPages(), num_rows(0), type(dtype), pages(), valid(true) {}
    ~column_t(){
        if (valid) {
            for(auto* page: pages){
                delete page;
            }
        }
    }

    column_t(const column_t&) = delete;
    column_t& operator=(const column_t&) = delete;

    column_t(column_t&& other) noexcept
        : num_rows(other.num_rows), type(other.type), threadPages(std::move(other.threadPages)) ,pages(std::move(other.pages)), valid(other.valid) {
        other.pages.clear();
        other.valid = true;
    }

    column_t& operator=(column_t&& other) noexcept {
        if (this != &other) {
            if (valid) {
                for (auto* page: pages) {
                    delete page;
                }
            }
            num_rows = other.num_rows;
            type  = other.type;
            pages = std::move(other.pages);
            threadPages = std::move(other.threadPages);
            valid = other.valid;
            other.pages.clear();
            other.valid = true;
        }
        return *this;
    }
    
    void insert_value_to_page(const value_t& entry) {

        // If the page is full or no page exists, create a new page and initialize the row count
        size_t max_rows = (PAGE_SIZE - sizeof(uint16_t)) / sizeof(value_t); 
        if (pages.empty() || *reinterpret_cast<uint16_t*>(pages.back()->data) >= max_rows) {
            pages.push_back(new Page());
            *reinterpret_cast<uint16_t*>(pages.back()->data) = 0;
        }
        
        //Get the pointer to the last page and insert the value_t in the page
        Page* page = pages.back();
        uint16_t& row_number = *reinterpret_cast<uint16_t*>(page->data);
        auto* buf = reinterpret_cast<value_t*>(page->data + sizeof(uint16_t));
        buf[row_number] = entry;
        ++row_number;
    }
    void insert_value_to_page(const value_t &entry, size_t tid)
    {
        size_t max_rows = (PAGE_SIZE - sizeof(uint16_t)) / sizeof(value_t); 
        assert(tid < threadPages.size() && "tid must be less that the size of threadPages");
        auto &pageVector = threadPages[tid];
        // If the page is full or no page exists, create a new page and initialize the row count
        if (pageVector.empty() || *reinterpret_cast<uint16_t *>(pageVector.back()->data) >= max_rows)
        {
            pageVector.push_back(new Page());
            *reinterpret_cast<uint16_t *>(pageVector.back()->data) = 0;
        }

        // Get the pointer to the last page and insert the value_t in the page
        Page *page = pageVector.back();
        uint16_t &row_number = *reinterpret_cast<uint16_t *>(page->data);
        auto *buf = reinterpret_cast<value_t *>(page->data + sizeof(uint16_t));
        buf[row_number] = entry;
        ++row_number;
    }

    void prepare_for_threads(size_t numThreads){
        if (threadPages.size() < numThreads) {
            threadPages.resize(numThreads);
        }
    }

    void collect_vectors()
    {
        assert(!collected && "Page vector has already been collected");
        size_t totalPages = 0;
        for (auto &v : threadPages)
            totalPages += v.size();

        pages.reserve(totalPages);
        // Use move iterators to avoid expensive copies
        for (auto& v : threadPages){ 
            pages.insert(pages.end(), std::make_move_iterator(v.begin()), std::make_move_iterator(v.end()));
            v.clear();
        }
    }
     
};

std::vector<column_t> scan_column_table(const ColumnarTable& table,
    const std::vector<std::tuple<size_t, DataType>>& output_attrs, const size_t& table_id);

ColumnarTable convert_column_t_to_columnar(
    const std::vector<column_t>& results,
    const Plan& plan,
    const std::vector<DataType>& types);



