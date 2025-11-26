#include <table.h>
struct value_t{
    //If data_idx if 0xFFFF then entry is null, if data_idx is 0xFFFE then entry is null and table_idx is the 16 lower bits
    // of the int entry and column_idx is the 16 higher order bits
    uint16_t table_idx;
    uint16_t column_idx;
    uint16_t page_idx;
    uint16_t data_idx;
};


// Structs for the second part of the project (columnar storage)
struct buffer_page{

    std:: vector<value_t> values;
    DataType           type;
    size_t  capacity;
};

struct column_t{
    std::vector<buffer_page*>    pages;
    size_t     total_rows=0 ; // initialize total_rows to 0
};

std::vector<std::vector<value_t>> scan_table(const ColumnarTable& ,
     const std::vector<std::tuple<size_t, DataType>>& , const size_t& );

std::vector<std::vector<Data>> materialize_table(const std::vector<std::vector<value_t>>& , const Plan& );

ColumnarTable materialize_columnar_table(const std::vector<std::vector<value_t>>& , 
                                        const Plan& , 
                                        const std::vector<DataType>& );