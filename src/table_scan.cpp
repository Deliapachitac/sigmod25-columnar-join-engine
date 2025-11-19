#include <table_scan.h>
#include <table.h>
#include <inner_column.h>
bool get_bitmap(const uint8_t* bitmap, uint16_t idx) {
    auto byte_idx = idx / 8;
    auto bit      = idx % 8;
    return bitmap[byte_idx] & (1u << bit);
}

value_t init_null(){
    value_t record;
    record.table_idx = 0;
    record.column_idx = 0;
    record.page_idx = 0;
    record.data_idx = 0xFFFF; //Indicates null value, should be converted to monostate after table materialization
    return record;
}


std::vector<std::vector<value_t>> scan_table(const ColumnarTable& table,
     const std::vector<std::tuple<size_t, DataType>>& output_attrs, const size_t& table_id) {
    namespace views = ranges::views;
    std::vector<std::vector<value_t>> results(table.num_rows,
        std::vector<value_t>(output_attrs.size(), init_null()));
    auto task = [&](size_t begin, size_t end) {
        size_t col_pap = 0;
        for (size_t column_idx = begin; column_idx < end; ++column_idx) {
            size_t in_col_idx = std::get<0>(output_attrs[column_idx]);
            auto& column = table.columns[in_col_idx];
            size_t row_idx = 0;
            uint16_t page_idx = 0;
            for (auto* page:
                column.pages | views::transform([](auto* page) { return page->data; })) {
                switch (column.type) {
                case DataType::INT32: {
                    auto  num_rows   = *reinterpret_cast<uint16_t*>(page);
                    auto* data_begin = reinterpret_cast<int32_t*>(page + 4);
                    auto* bitmap =
                        reinterpret_cast<uint8_t*>(page + PAGE_SIZE - (num_rows + 7) / 8);
                    uint16_t data_idx = 0;
                    for (uint16_t i = 0; i < num_rows; ++i) {
                        if (get_bitmap(bitmap, i)) {
                            auto value = data_begin[data_idx++];
                            if (row_idx >= table.num_rows) {
                                throw std::runtime_error("row_idx");
                            }
                            results[row_idx][column_idx].data_idx = 0xFFFE; //0xFFFE indicates int, bits will be stored in table and column_idx
                            results[row_idx][column_idx].table_idx = value & 0xFFFF; //Lower 16 bits of value
                            results[row_idx][column_idx].column_idx = (value >> 16) & 0xFFFF; //Higher 16 bits of value;
                            row_idx++;
                        } else {
                            ++row_idx;
                        }
                    }
                    break;
                }
                case DataType::VARCHAR: {
                    auto num_rows = *reinterpret_cast<uint16_t*>(page);
                    if (num_rows == 0xffff) {
                        if (row_idx >= table.num_rows) {
                            throw std::runtime_error("row_idx");
                        }
                        results[row_idx][column_idx].table_idx = table_id;
                        results[row_idx][column_idx].column_idx = in_col_idx;
                        results[row_idx][column_idx].page_idx = page_idx;
                        results[row_idx][column_idx].data_idx = 0;
                        row_idx++;
                    }else if (num_rows != 0xfffe) {
                        auto* bitmap =
                            reinterpret_cast<uint8_t*>(page + PAGE_SIZE - (num_rows + 7) / 8);
                        uint16_t data_idx = 0;
                        for (uint16_t i = 0; i < num_rows; ++i) {
                            if (get_bitmap(bitmap, i)) {
                                if (row_idx >= table.num_rows) {
                                    throw std::runtime_error("row_idx");
                                }
                                results[row_idx][column_idx].table_idx = table_id;
                                results[row_idx][column_idx].column_idx = in_col_idx;
                                results[row_idx][column_idx].page_idx = page_idx;
                                results[row_idx][column_idx].data_idx = data_idx;
                                data_idx++;
                                row_idx++;
                            } else {
                                ++row_idx;
                            }
                        }
                    }
                    break;
                }
                }
                page_idx++;
            }
        }
    };
    filter_tp.run(task, output_attrs.size());
    return results;
}