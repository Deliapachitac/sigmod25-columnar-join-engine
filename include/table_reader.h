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
struct Column {
    DataType  type;         
    std::vector<Page*> pages;

    Column(DataType t) : type(t) {}

    Page* new_page() {
        auto ret = new Page;
        pages.push_back(ret);
        return ret;
    }

    //constructor
    Column(DataType data_type): type(data_type), pages() {}

    //destructor
    ~Column() {
        for (auto* p : pages) {
            delete p;
        }
    }

    
    // Column(const Column&) = delete;
    // Column& operator=(const Column&) = delete;

    // Column(Column&& other) noexcept
    //     : type(other.type), pages(std::move(other.pages)) {
    //     other.pages.clear();
    // }

    // Column& operator=(Column&& other) noexcept {
    //     if (this != &other) {
    //         for (auto* p : pages) delete p;
    //         type = other.type;
    //         pages = std::move(other.pages);
    //         other.pages.clear();
    //     }
    //     return *this;
    // }

};     

struct column_t {
    size_t num_rows;  // number of rows in the table 
    std::vector<Column> columns;  
};


struct MyColumnInserter {
    Column& column;           
    size_t last_page_idx = 0;       
    size_t offset = 0;   //current write offset inside page->data

    MyColumnInserter(Column& col) : column(col) {}

    Page* get_page() {
        if (last_page_idx == column.pages.size()) {
            column.new_page();
        }       
        return column.pages[last_page_idx];
    }


    void insert_value(uint16_t table_idx, uint16_t col_idx, uint16_t data_idx) {
        Page* page = get_page();

        //check if we have enough space in the current page
        if ((offset + 1) * sizeof(value_t) > PAGE_SIZE) {
            ++last_page_idx;
            offset = 0;
            page = get_page();
        }

        auto* buf = reinterpret_cast<value_t*>(page->data);
        buf[offset] = value_t{
            .table_idx  = table_idx,
            .column_idx = col_idx,
            .page_idx   = static_cast<uint16_t>(last_page_idx),
            .data_idx   = data_idx
        };
        ++offset;
    }

    void insert_null(uint16_t table_idx, uint16_t col_idx) {
        Page* page = get_page();

        //check if we have enough space in the current page
        if ((offset + 1) * sizeof(value_t) > PAGE_SIZE) {
            ++last_page_idx;
            offset = 0;
            page = get_page();
        }

        auto* buf = reinterpret_cast<value_t*>(page->data);
        buf[offset] = value_t{
            .table_idx  = table_idx,
            .column_idx = col_idx,
            .page_idx   = static_cast<uint16_t>(last_page_idx),
            .data_idx   = 0xFFFF // this is  NULL
        };
        ++offset;
    }

    void finalize() {
        
    }
};

std::vector<column_t> scan_column_table(const ColumnarTable& table,
     const std::vector<std::tuple<size_t, DataType>>& output_attrs, const size_t& table_id);





