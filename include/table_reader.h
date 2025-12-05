#include <table.h>

struct value_t{
    //If data_idx if 0xFFFF then entry is null, if data_idx is 0xFFFE then entry is null and table_idx is the 16 lower bits
    // of the int entry and column_idx is the 16 higher order bits
    uint16_t table_idx;
    uint16_t column_idx;
    uint16_t page_idx;
    uint16_t data_idx;
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
    std::vector<Page*> pages;
    
    column_t(DataType dtype) : num_rows(0), type(dtype), pages() {}
    ~column_t(){
        for(auto* page: pages){
            delete page;
        }
    }

    column_t(const column_t&) = delete;
    column_t& operator=(const column_t&) = delete;

    column_t(column_t&& other) noexcept
        : num_rows(other.num_rows), type(other.type), pages(std::move(other.pages)) {
        other.pages.clear();
    }

    column_t& operator=(column_t&& other) noexcept {
        if (this != &other) {
            for (auto* page: pages) {
                delete page;
            }
            num_rows = other.num_rows;
            type  = other.type;
            pages = std::move(other.pages);
            other.pages.clear();
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

     
};

std::vector<column_t> scan_column_table(const ColumnarTable& table,
    const std::vector<std::tuple<size_t, DataType>>& output_attrs, const size_t& table_id);

ColumnarTable convert_column_t_to_columnar(
    const std::vector<column_t>& results,
    const Plan& plan,
    const std::vector<DataType>& types);



