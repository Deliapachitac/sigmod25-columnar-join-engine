#include <table.h>
struct value_t{
    //If data_idx if 0xFFFF then entry is null, if data_idx is 0xFFFE then entry is null and table_idx is the 16 lower bits
    // of the int entry and column_idx is the 16 higher order bits
    uint16_t table_idx;
    uint16_t column_idx;
    uint16_t page_idx;
    uint16_t data_idx;
};

std::vector<std::vector<value_t>> scan_table(const ColumnarTable& table,
     const std::vector<std::tuple<size_t, DataType>>& output_attrs, const size_t& table_id);

std::vector<std::vector<Data>> materialize_table(const std::vector<std::vector<value_t>>& table, const Plan& plan);