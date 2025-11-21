#include <table_reader.h>
#include <table.h>
#include <iostream>
#include <inner_column.h>
namespace helper{
    constexpr uint16_t NULL_VALUE = 0xFFFF;
    constexpr uint16_t INT_VALUE     = 0xFFFE;

    inline bool get_bitmap(const uint8_t* bitmap, uint16_t idx) {
        auto byte_idx = idx / 8;
        auto bit      = idx % 8;
        return bitmap[byte_idx] & (1u << bit);
    }

    value_t init_null(){
        value_t record;
        record.table_idx = 0;
        record.column_idx = 0;
        record.page_idx = 0;
        record.data_idx = NULL_VALUE; 
        return record;
    }

    inline void ensure_bitmap_size(std::vector<uint8_t>& bitmap, uint16_t idx) {
        size_t needed = idx / 8 + 1;
        if (bitmap.size() < needed) bitmap.resize(needed);
    }
    inline void set_bitmap(std::vector<uint8_t>& bitmap, uint16_t idx) {
        ensure_bitmap_size(bitmap, idx);
        auto byte_idx = idx / 8;
        auto bit      = idx % 8;
        bitmap[byte_idx] |= static_cast<uint8_t>(1u << bit);
    }

    inline void unset_bitmap(std::vector<uint8_t>& bitmap, uint16_t idx) {
        ensure_bitmap_size(bitmap, idx);
        auto byte_idx = idx / 8;
        auto bit      = idx % 8;
        bitmap[byte_idx] &= static_cast<uint8_t>(~(1u << bit));
    }
};

std::vector<std::vector<value_t>> scan_table(const ColumnarTable& table,
     const std::vector<std::tuple<size_t, DataType>>& output_attrs, const size_t& table_id) {
    namespace views = ranges::views;
    std::vector<std::vector<value_t>> results(table.num_rows,
        std::vector<value_t>(output_attrs.size(), helper::init_null()));
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
                        if (helper::get_bitmap(bitmap, i)) {
                            auto value = data_begin[data_idx++];
                            if (row_idx >= table.num_rows) {
                                throw std::runtime_error("row_idx");
                            }
                            results[row_idx][column_idx].data_idx = helper::INT_VALUE; //0xFFFE indicates int, bits will be stored in table and column_idx
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
                            if (helper::get_bitmap(bitmap, i)) {
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
    if(table.size() == 0){
        return std::vector<std::vector<Data>>{};
    }
    std::vector<std::vector<Data>> res(table.size(),
        std::vector<Data>(table[0].size(), std::monostate{}));
    for(size_t row = 0; row < table.size(); row++){
        for(size_t column = 0; column < table[0].size(); column++ ){
            const value_t& entry = table[row][column];
            //If value is null, leave it as monostate
            if(entry.data_idx == helper::NULL_VALUE){
                continue;
            }
            //If value is int, combine fields and emplace it into result
            else if (entry.data_idx == helper::INT_VALUE){
                int32_t value =
                    (static_cast<int32_t>(entry.column_idx) << 16) |
                    static_cast<int32_t>(entry.table_idx);
                res[row][column].emplace<int32_t>(value);
            }
            //Value is string
            else{
                uint16_t page_idx = entry.page_idx;
                auto& page_vector = plan.inputs[entry.table_idx].columns[entry.column_idx].pages;
                auto* page = page_vector[page_idx++]->data;
                auto num_rows = *reinterpret_cast<uint16_t*>(page);
                //Long string handling error
                if (num_rows == 0xffff) {
                    auto        num_chars  = *reinterpret_cast<uint16_t*>(page + 2);
                    auto*       data_begin = reinterpret_cast<char*>(page + 4);
                    std::string value{data_begin, data_begin + num_chars};
                    res[row][column].emplace<std::string>(std::move(value));
                    while (page_idx < page_vector.size()){
                        page = page_vector[page_idx++]->data;
                        num_rows = *reinterpret_cast<uint16_t*>(page);
                        if(num_rows != 0xfffe) break;
                        num_chars  = *reinterpret_cast<uint16_t*>(page + 2);
                        data_begin = reinterpret_cast<char*>(page + 4);
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
                    }
                }
                else {
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

void materialize_string(const value_t string_meta, const Plan& plan, std::string& value){
    uint16_t page_idx = string_meta.page_idx;
    auto& page_vector = plan.inputs[string_meta.table_idx].columns[string_meta.column_idx].pages;
    auto* page = page_vector[page_idx++]->data;
    auto num_rows = *reinterpret_cast<uint16_t*>(page);
    //Long string handling error
    if (num_rows == 0xffff) {
        auto        num_chars  = *reinterpret_cast<uint16_t*>(page + 2);
        auto*       data_begin = reinterpret_cast<char*>(page + 4);
        value = std::string{data_begin, data_begin + num_chars};
        while (page_idx < page_vector.size()){
            page = page_vector[page_idx++]->data;
            num_rows = *reinterpret_cast<uint16_t*>(page);
            if(num_rows != 0xfffe) break;
            num_chars  = *reinterpret_cast<uint16_t*>(page + 2);
            data_begin = reinterpret_cast<char*>(page + 4);
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::string>) {
                value.insert(value.end(), data_begin, data_begin + num_chars);
            } else {
                throw std::runtime_error(
                    "long string page 0xfffe must follow a string");
            }     
        }
    }
    else {
        auto  num_non_null = *reinterpret_cast<uint16_t*>(page + 2);
        auto* offset_begin = reinterpret_cast<uint16_t*>(page + 4);
        auto* data_begin   = reinterpret_cast<char*>(page + 4 + num_non_null * 2);
        auto  old_offset = string_meta.data_idx ? offset_begin[(string_meta.data_idx)-1] : 0;
        auto* string_begin = data_begin + old_offset;
        auto  offset = offset_begin[string_meta.data_idx];
        
        value = std::string{string_begin, data_begin + offset};
    }
}
ColumnarTable materialize_columnar_table(const std::vector<std::vector<value_t>>& table, 
                                        const Plan& plan, 
                                        const std::vector<DataType>& data_types
    ) {
    namespace views  = ranges::views;
    ColumnarTable ret;
    ret.num_rows = table.size();
    for (auto [col_idx, data_type]: data_types | views::enumerate) {
        ret.columns.emplace_back(data_type);
        auto& column = ret.columns.back();
        switch (data_type) {
        case DataType::INT32: {
            uint16_t             num_rows = 0;
            std::vector<int32_t> data;
            std::vector<uint8_t> bitmap;
            data.reserve(2048);
            bitmap.reserve(256);
            auto save_page = [&column, &num_rows, &data, &bitmap]() {
                auto* page                             = column.new_page()->data;
                *reinterpret_cast<uint16_t*>(page)     = num_rows;
                *reinterpret_cast<uint16_t*>(page + 2) = static_cast<uint16_t>(data.size());
                memcpy(page + 4, data.data(), data.size() * 4);
                memcpy(page + PAGE_SIZE - bitmap.size(), bitmap.data(), bitmap.size());
                num_rows = 0;
                data.clear();
                bitmap.clear();
            };
            for (auto& record: table) {
                auto& value = record[col_idx];
                if (value.data_idx == helper::INT_VALUE) {
                    if (4 + (data.size() + 1) * 4 + (num_rows / 8 + 1) > PAGE_SIZE) {
                        save_page();
                    }
                    helper::set_bitmap(bitmap, num_rows);
                    auto combined_value = ((int32_t(value.column_idx) & 0xFFFF) << 16) |
                                (int32_t(value.table_idx)  & 0xFFFF);
                    data.emplace_back(combined_value);
                    ++num_rows;
                } else if (value.data_idx == helper::NULL_VALUE) {
                    if (4 + (data.size()) * 4 + (num_rows / 8 + 1) > PAGE_SIZE) {
                        save_page();
                    }
                    helper::unset_bitmap(bitmap, num_rows);
                    ++num_rows;
                }
                   
            }
            if (num_rows != 0) {
                save_page();
            }
            break;
        }
        case DataType::VARCHAR: {
            uint16_t              num_rows = 0;
            std::vector<char>     data;
            std::vector<uint16_t> offsets;
            std::vector<uint8_t>  bitmap;
            data.reserve(8192);
            offsets.reserve(4096);
            bitmap.reserve(512);
            auto save_long_string = [&column](std::string_view data) {
                size_t offset     = 0;
                auto   first_page = true;
                while (offset < data.size()) {
                    auto* page = column.new_page()->data;
                    if (first_page) {
                        *reinterpret_cast<uint16_t*>(page) = 0xffff;
                        first_page                         = false;
                    } else {
                        *reinterpret_cast<uint16_t*>(page) = 0xfffe;
                    }
                    auto page_data_len = std::min(data.size() - offset, PAGE_SIZE - 4);
                    *reinterpret_cast<uint16_t*>(page + 2) = page_data_len;
                    memcpy(page + 4, data.data() + offset, page_data_len);
                    offset += page_data_len;
                }
            };
            auto save_page = [&column, &num_rows, &data, &offsets, &bitmap]() {
                auto* page                             = column.new_page()->data;
                *reinterpret_cast<uint16_t*>(page)     = num_rows;
                *reinterpret_cast<uint16_t*>(page + 2) = static_cast<uint16_t>(offsets.size());
                memcpy(page + 4, offsets.data(), offsets.size() * 2);
                memcpy(page + 4 + offsets.size() * 2, data.data(), data.size());
                memcpy(page + PAGE_SIZE - bitmap.size(), bitmap.data(), bitmap.size());
                num_rows = 0;
                data.clear();
                offsets.clear();
                bitmap.clear();
            };
            for (auto& record: table) {
                auto& string_meta = record[col_idx];
                
                
                if (string_meta.data_idx != helper::NULL_VALUE && string_meta.data_idx != helper::INT_VALUE) {
                    std::string value;
                    materialize_string(string_meta, plan, value);
                    if (value.size() > PAGE_SIZE - 7) {
                        if (num_rows > 0) {
                            save_page();
                        }
                        save_long_string(value);
                    } else {
                        if (4 + (offsets.size() + 1) * 2 + (data.size() + value.size())
                                + (num_rows / 8 + 1)
                            > PAGE_SIZE) {
                            save_page();
                        }
                        helper::set_bitmap(bitmap, num_rows);
                        data.insert(data.end(), value.begin(), value.end());
                        offsets.emplace_back(data.size());
                        ++num_rows;
                    }
                } else if (string_meta.data_idx == helper::NULL_VALUE) {
                    if (4 + offsets.size() * 2 + data.size() + (num_rows / 8 + 1)
                        > PAGE_SIZE) {
                        save_page();
                    }
                    helper::unset_bitmap(bitmap, num_rows);
                    ++num_rows;
                } else {
                    throw std::runtime_error("not string or null");
                }
                    
            }
            if (num_rows != 0) {
                save_page();
            }
            break;
        }
        }
    }
    return ret;
}