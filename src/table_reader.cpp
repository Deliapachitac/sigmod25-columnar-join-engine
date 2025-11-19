#include <table_reader.h>
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

std::vector<std::vector<Data>> materialize_table(const std::vector<std::vector<value_t>>& table, const Plan& plan){
    std::vector<std::vector<Data>> res(table.size(),
        std::vector<Data>(table[0].size(), std::monostate{}));
    for(int row = 0; row < table.size(); row++){
        for(int column = 0; column < table[0].size(); column++ ){
            const value_t& entry = table[row][column];
            //If value is null, leave it as monostate
            if(entry.data_idx == 0xFFFF){
                continue;
            }
            //If value is int, combine fields and emplace it into result
            else if (entry.data_idx == 0xFFFE){
                int32_t value =
                    (static_cast<int32_t>(entry.column_idx) << 16) |
                    static_cast<int32_t>(entry.table_idx);
                res[row][column].emplace<int32_t>(value);
            }
            //Value is string
            else{
                auto* page = plan.inputs[entry.table_idx].columns[entry.column_idx].pages[entry.page_idx]->data;
                auto num_rows = *reinterpret_cast<uint16_t*>(page);
                if (num_rows == 0xffff) {
                    auto        num_chars  = *reinterpret_cast<uint16_t*>(page + 2);
                    auto*       data_begin = reinterpret_cast<char*>(page + 4);
                    std::string value{data_begin, data_begin + num_chars};
                    res[row][column].emplace<std::string>(std::move(value));
                } else if (num_rows == 0xfffe) {
                    auto  num_chars  = *reinterpret_cast<uint16_t*>(page + 2);
                    auto* data_begin = reinterpret_cast<char*>(page + 4);
                    std::visit(
                        [data_begin, num_chars](auto& value) {
                            using T = std::decay_t<decltype(value)>;
                            if constexpr (std::is_same_v<T, std::string>) {
                                value.insert(value.end(), data_begin, data_begin + num_chars);
                            } else {
                                throw std::runtime_error(
                                    "long string page 0xfffe must follow a string");
                            }
                        },
                        res[row][column]);
                } else {
                    auto  num_non_null = *reinterpret_cast<uint16_t*>(page + 2);
                    auto* offset_begin = reinterpret_cast<uint16_t*>(page + 4);
                    auto* data_begin   = reinterpret_cast<char*>(page + 4 + num_non_null * 2);
                    auto  old_offset = entry.data_idx ? offset_begin[(entry.data_idx)-1] : 0;
                    auto* string_begin = data_begin + old_offset;
                    auto  offset = offset_begin[entry.data_idx];
                    
                    std::string value{string_begin, data_begin + offset};

                    res[row][column].emplace<std::string>(std::move(value));
                }
            }
        }
    }
    return res;
}