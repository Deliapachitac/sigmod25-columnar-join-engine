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
        HashTable hash_table;
        

        size_t max_rows_page= (PAGE_SIZE - sizeof(uint16_t)) / sizeof(value_t);

        // The build_left decided which side to build the hash table on row store 
        //Now in column store the key is just in one column so it does not matter which side we build on
        //choose build side
        auto& build_key = build_left ? left[left_col] : right[right_col];
        auto& probe_key = build_left ? right[right_col] : left[left_col];

        //Iterates over the pages of the join column and hash the entry key
        for (size_t p = 0; p < build_key.columns[0].pages.size(); ++p) {
            
            // extract the page , number of rows and the entries in a buffer
            Page* page = build_key.columns[0].pages[p];
            uint16_t num_rows = *reinterpret_cast<uint16_t*>(page->data);
            auto* page_buffer = reinterpret_cast<value_t*>(page->data + sizeof(uint16_t));

            for (size_t i = 0; i < num_rows; ++i) {
                value_t record = page_buffer[i];
                if (record.data_idx == 0xFFFF) continue;
                int32_t key = static_cast<int32_t>(record.data_idx);
                size_t row_index = p * max_rows_page + i; 
                hash_table[key].push_back(row_index); 
           
            }
                
        }

        // Prepare inserters for each output column
        std::vector<MyColumnInserter> inserters;
        inserters.reserve(output_attrs.size());
        for (size_t out_idx = 0; out_idx < output_attrs.size(); ++out_idx) {
            inserters.emplace_back(results[out_idx].columns[0]);
        }

        //
        size_t out_rows = 0;
        for (size_t p = 0; p < probe_key.columns[0].pages.size(); ++p) {

            // extract the page , number of rows and the entries in a buffer        
            Page* page = probe_key.columns[0].pages[p];
            uint16_t num_rows = *reinterpret_cast<uint16_t*>(page->data);
            auto* page_buffer = reinterpret_cast<value_t*>(page->data + sizeof(uint16_t));

            for (uint16_t i = 0; i < num_rows; ++i) {
                const value_t probe_v = page_buffer[i];
                if (probe_v.data_idx == 0xFFFF) continue;

                int32_t key = static_cast<int32_t>(probe_v.data_idx);
                auto it = hash_table.find(key);
                if (it == hash_table.end()) continue;

                for (size_t row_id : it->second) {
                    size_t build_page_idx = row_id / max_rows_page;
                    size_t build_slot     = row_id % max_rows_page;

                    Page* build_page = build_key.columns[0].pages[build_page_idx];
                    auto* build_buf = reinterpret_cast<value_t*>(build_page->data + sizeof(uint16_t));
                    const value_t build_v = build_buf[build_slot];

                    // Emit one joined row
                    for (size_t out_idx = 0; out_idx < output_attrs.size(); ++out_idx) {
                        auto& inserter = inserters[out_idx];
                        size_t in_attr = std::get<0>(output_attrs[out_idx]);

                        if (in_attr < left.size()) {
                            // Attribute from left side
                            const value_t src = (build_left ? build_v : probe_v);
                            if (src.data_idx == 0xFFFF)
                                inserter.insert_null(src.table_idx, src.column_idx);
                            else
                                inserter.insert_value(src.table_idx, src.column_idx, src.data_idx);
                        } else {
                            // Attribute from right side
                            const value_t src = (build_left ? probe_v : build_v);
                            if (src.data_idx == 0xFFFF)
                                inserter.insert_null(src.table_idx, src.column_idx);
                            else
                                inserter.insert_value(src.table_idx, src.column_idx, src.data_idx);
                        }
                    }
                    ++out_rows;
                }
            }
            

        }

        // 
        for (auto& colset : results) {
            colset.num_rows = out_rows;
        }

               
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

    
    results.resize(output_attrs.size());
    for (size_t i = 0; i < output_attrs.size(); ++i) {
        auto type = std::get<1>(output_attrs[i]);
        results[i].num_rows = 0;
        results[i].columns.emplace_back(MyColumn(type)); 
    }

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