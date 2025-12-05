#include <hardware.h>
#include <plan.h>
#include <table.h>
#include <iostream>
#include <robin_hood.h>
#include <table_reader.h>
#include <cuckoo_hash.h>
#include <hopscotch.h>
#include <cstdlib>
// SET TO 1 TO USE THIS HASHMAP, IF MULTIPLE ARE ACTIVE THE FIRST IN ORDER WILL BE USED, NONE ACTIVE AND UNORDERED_MAP WILL BE USED INSTEAD AS DEFAULT
#define USE_RH 0
#define USE_CUCKOO 0
#define USE_HOPSCOTCH 0

#if USE_RH
using HashTable = rh_map<int32_t, std::vector<size_t>>;
#elif USE_CUCKOO
using HashTable = cuckoo_map<int32_t, std::vector<size_t>>;
#elif USE_HOPSCOTCH
using HashTable = HopscotchMap<int32_t, std::vector<size_t>>;
#else
using HashTable = std::unordered_map<int32_t, std::vector<size_t>>;
#endif

namespace Contest {

using ExecuteResult = std::vector<column_t>; 

ExecuteResult execute_impl(const Plan& plan, size_t node_idx);

struct JoinAlgorithm {
    bool                                             build_left;
    ExecuteResult&                                   left;
    ExecuteResult&                                   right;
    ExecuteResult&                                   results;
    size_t                                           left_col, right_col;
    const std::vector<std::tuple<size_t, DataType>>& output_attrs;

    auto run() {
        namespace views = ranges::views;
        
        size_t sz = build_left ? left.size() : right.size();
        HashTable hash_table(sz);
        std::vector<std::pair<int32_t, size_t>> build_keys; // (key, row_idx)
        
        
        if (build_left) { 

            //extract keys from left side
            extract_keys_from_column(hash_table,left[left_col], build_keys );

            //probe phase
            probe_phase(hash_table, left, right, build_keys,  true);
            
        } else {
            
            //extract keys from right side
            extract_keys_from_column(hash_table,right[right_col], build_keys );

            //probe phase
            probe_phase(hash_table, left, right, build_keys,  false);
            
        }
    }

    private:

    void extract_keys_from_column(HashTable& hash_table, column_t& column, std::vector<std::pair<int32_t, size_t>>& build_keys){
        size_t row_idx = 0;
        for (auto& mycolumn: column.columns) {
            for(auto* page: mycolumn.pages) {

                uint16_t num_rows = *reinterpret_cast<uint16_t*>(page->data);
                auto* buf = reinterpret_cast<value_t*>(page->data + sizeof(uint16_t));
                
                for (uint16_t i = 0; i < num_rows; i++)
                {
                    const value_t& record = buf[i];
                    if(record.data_idx == 0xFFFF) {
                        row_idx++;
                        continue;
                    }
                
                
                    int32_t key = ((int32_t(record.column_idx) & 0xFFFF) << 16) |
                                (int32_t(record.table_idx)  & 0xFFFF);
                    build_keys.emplace_back(key, row_idx);
                    
                    if (key >= 0) {
                        auto itr = hash_table.find(key);
                        if (itr == hash_table.end()) {
                            hash_table.emplace(key, std::vector<size_t>(1, row_idx));
                        } else {
                            itr->second.push_back(row_idx);
                        }
                        row_idx++;
                    } else {
                        throw std::runtime_error("wrong type of field");
                    }
                }
            } 
        }
    }


    void probe_phase(HashTable& hash_table, ExecuteResult& left, ExecuteResult& right, const std::vector<std::pair<int32_t, size_t>>& build_keys, bool is_left) {
        
        // Initialize output column_t structures if not already done
        if (results.empty()) {
            for (auto [col_idx, dtype]: output_attrs) {
                column_t out;
                out.num_rows = 0;
                out.type = dtype;
                out.columns.emplace_back(MyColumn());
                results.push_back(std::move(out));
            }
        }
        
        // Create persistent inserters for all output columns
        std::vector<MyColumnInserter> inserters;
        for (auto& res_col : results) {
            inserters.emplace_back(res_col.columns[0]);
        }
        
        // Determine which column to probe based on build side
        column_t& probe_col = is_left ? right[right_col] : left[left_col];
        
        size_t probe_row_idx = 0;
        for (auto& mycolumn: probe_col.columns) {
            for(auto* page: mycolumn.pages) {
                uint16_t num_rows = *reinterpret_cast<uint16_t*>(page->data);
                auto* buf = reinterpret_cast<value_t*>(page->data + sizeof(uint16_t));
                
                for (uint16_t i = 0; i < num_rows; i++)
                {
                    const value_t& record = buf[i];
                    if(record.data_idx == 0xFFFF) {
                        probe_row_idx++;
                        continue;
                    }
                
                
                    int32_t key = ((int32_t(record.column_idx) & 0xFFFF) << 16) |
                                (int32_t(record.table_idx)  & 0xFFFF);
                    if (key >= 0) {
                        if (auto itr = hash_table.find(key); itr != hash_table.end()) {
                            for (auto build_row_idx: itr->second) {
                                // For each match, insert values into output columns
                                for (size_t out_col_idx = 0; out_col_idx < output_attrs.size(); ++out_col_idx) {
                                    size_t src_col_idx = std::get<0>(output_attrs[out_col_idx]);
                                    value_t value_to_insert;
                                    
                                    // Determine which side to pull from
                                    if (is_left) {
                                        // Build is left, probe is right
                                        if (src_col_idx < left.size()) {
                                            value_to_insert = get_value_at_row(left[src_col_idx], build_row_idx);
                                        } else {
                                            value_to_insert = get_value_at_row(right[src_col_idx - left.size()], probe_row_idx);
                                        }
                                    } else {
                                        // Build is right, probe is left
                                        if (src_col_idx < left.size()) {
                                            value_to_insert = get_value_at_row(left[src_col_idx], probe_row_idx);
                                        } else {
                                            value_to_insert = get_value_at_row(right[src_col_idx - left.size()], build_row_idx);
                                        }
                                    }
                                    
                                    // Insert using persistent inserter
                                    inserters[out_col_idx].insert_value(
                                        value_to_insert.table_idx,
                                        value_to_insert.column_idx,
                                        value_to_insert.page_idx,
                                        value_to_insert.data_idx);
                                }
                                
                                // Increment row count for first result column only (they're all the same)
                                results[0].num_rows++;
                            }
                        }
                        probe_row_idx++;
                    } else {
                        throw std::runtime_error("wrong type of field");
                    }
                }
            }
        }
        
        // Sync num_rows across all result columns
        for (size_t i = 1; i < results.size(); ++i) {
            results[i].num_rows = results[0].num_rows;
        }
    }
    
    value_t get_value_at_row(const column_t& col, size_t row_idx) {
        size_t current_row = 0;
        for ( auto& mycolumn : col.columns) {
            for ( auto* page : mycolumn.pages) {
                uint16_t num_rows = *reinterpret_cast<uint16_t*>(page->data);
                auto* buf = reinterpret_cast<value_t*>(page->data + sizeof(uint16_t));
                
                if (current_row + num_rows > row_idx) {
                    return buf[row_idx - current_row];
                }
                current_row += num_rows;
            }
        }
        throw std::runtime_error("row_idx out of bounds");
    }
    
};

ExecuteResult execute_hash_join(const Plan&          plan,
    const JoinNode&                                  join,
    const std::vector<std::tuple<size_t, DataType>>& output_attrs) {
    auto                           left_idx    = join.left;
    auto                           right_idx   = join.right;
    auto&                          left_node   = plan.nodes[left_idx];
    auto&                          right_node  = plan.nodes[right_idx];
    auto&                          left_types  = left_node.output_attrs;
    auto&                          right_types = right_node.output_attrs;
    auto                           left        = execute_impl(plan, left_idx);
    auto                           right       = execute_impl(plan, right_idx);
    std::vector<column_t> results;

    JoinAlgorithm join_algorithm{.build_left = join.build_left,
        .left                                = left,
        .right                               = right,
        .results                             = results,
        .left_col                            = join.left_attr,
        .right_col                           = join.right_attr,
        .output_attrs                        = output_attrs};

    join_algorithm.run();

    return results;
}

ExecuteResult execute_scan(const Plan&               plan,
    const ScanNode&                                  scan,
    const std::vector<std::tuple<size_t, DataType>>& output_attrs) {
    auto                           table_id = scan.base_table_id;
    auto&                          input    = plan.inputs[table_id];
    return scan_column_table(input, output_attrs, table_id); 
}

ExecuteResult execute_impl(const Plan& plan, size_t node_idx) {
    auto& node = plan.nodes[node_idx];
    return std::visit(
        [&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, JoinNode>) {
                return execute_hash_join(plan, value, node.output_attrs);
            } else {
                return execute_scan(plan, value, node.output_attrs);
            }
        },
        node.data);
}

ColumnarTable execute(const Plan& plan, [[maybe_unused]] void* context) {
    namespace views = ranges::views;
    auto ret        = execute_impl(plan, plan.root);
    auto ret_types  = plan.nodes[plan.root].output_attrs
                   | views::transform([](const auto& v) { return std::get<1>(v); })
                   | ranges::to<std::vector<DataType>>();
    return convert_column_t_to_columnar(ret, plan, ret_types); 
}
void* build_context() {
    return nullptr;
}

void destroy_context([[maybe_unused]] void* context) {}

} // namespace Contest
