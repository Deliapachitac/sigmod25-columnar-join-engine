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
struct MyColumn {
    DataType  type;         
    std::vector<Page*> pages; // first 2 bytes the number of rows, next bytes the value_t entries

    Page* new_page() {
        auto ret = new Page;
        *reinterpret_cast<uint16_t*>(ret->data) = 0; // initialise row count to 0
        pages.push_back(ret);
        return ret;
    }

    //constructor
    MyColumn(DataType data_type): type(data_type), pages() {}

    //destructor
    ~MyColumn() {
        for (auto* p : pages) {
            delete p;
        }
    }

    
    MyColumn(const MyColumn&) = delete;
    MyColumn& operator=(const MyColumn&) = delete;

    MyColumn(MyColumn&& other) noexcept
        : type(other.type), pages(std::move(other.pages)) {
        other.pages.clear();
    }

    MyColumn& operator=(MyColumn&& other) noexcept {
        if (this != &other) {
            for (auto* p : pages) delete p;
            type = other.type;
            pages = std::move(other.pages);
            other.pages.clear();
        }
        return *this;
    }

};     

struct column_t {
    size_t num_rows;  // number of rows in the table 
    std::vector<MyColumn> columns;  
};


struct MyColumnInserter {
    MyColumn& mycolumn;           
    size_t last_page_idx = 0; 
    
    MyColumnInserter(MyColumn& col) : mycolumn(col) {}
    
    Page* get_page() {
        if (last_page_idx == mycolumn.pages.size()) {
            Page* p = mycolumn.new_page();
            *reinterpret_cast<uint16_t*>(p->data) = 0;
        }       
        return mycolumn.pages[last_page_idx];
    }


    void insert_value(uint16_t table_idx, uint16_t col_idx, uint16_t data_idx) {
        
        // First we need to get the current page and the current number of rows
        Page* page = get_page();
        uint16_t& num_rows = *reinterpret_cast<uint16_t*>(page->data);

        //check if we have enough space in the current page 
        // If we don't have enough space we need to get a new page
        size_t max_rows = (PAGE_SIZE - sizeof(uint16_t)) / sizeof(value_t);
        if (num_rows >= max_rows) {
            ++last_page_idx;
            page = get_page();
            num_rows = *reinterpret_cast<uint16_t*>(page->data); 
        }

        // Each entry is of size value_t so we are adding indexes to the data 
        auto* buf = reinterpret_cast<value_t*>(page->data+ sizeof(uint16_t));
        buf[num_rows] = value_t{
            .table_idx = table_idx,
            .column_idx = col_idx,
            .page_idx = static_cast<uint16_t>(last_page_idx),
            .data_idx=  data_idx
        };

        // increase the number of rows because we added a new entry
        ++num_rows; 
    }

    void insert_null(uint16_t table_idx, uint16_t col_idx) {
        
        Page* page= get_page();
        uint16_t& num_rows = *reinterpret_cast<uint16_t*>(page->data);
                            
        //check if we have enough space in the current page
        //If we don't have enough space we need to get a new page
        size_t max_rows = (PAGE_SIZE - sizeof(uint16_t)) / sizeof(value_t);
        if (num_rows >= max_rows) {
            ++last_page_idx;
            page =  get_page();
            num_rows  = *reinterpret_cast<uint16_t*>(page->data);
        }            

        auto* buf = reinterpret_cast<value_t*>(page->data + sizeof(uint16_t));
        buf[num_rows] = value_t{
            .table_idx = table_idx,
            .column_idx = col_idx,
            .page_idx =static_cast<uint16_t>(last_page_idx),
            .data_idx =  0xFFFF // this is  NULL
        };
        ++num_rows;                
    }        

};

std::vector<column_t> scan_column_table(const ColumnarTable& table,
     const std::vector<std::tuple<size_t, DataType>>& output_attrs, const size_t& table_id);

ColumnarTable convert_column_t_to_columnar(
    std::vector<column_t>& results,
    const Plan& plan,
    const std::vector<DataType>& types);



