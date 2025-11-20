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

using ExecuteResult = std::vector<std::vector<value_t>>; 


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

        
        if (build_left) { 
            for (auto&& [idx, record]: left | views::enumerate) {
                if(record[left_col].data_idx == 0xFFFF) continue;
                auto key = ((int32_t(record[left_col].column_idx) & 0xFFFF) << 16) |
                                (int32_t(record[left_col].table_idx)  & 0xFFFF);
                if (key >= 0) {
                    if (auto itr = hash_table.find(key); itr == hash_table.end()) {
                        hash_table.emplace(key, std::vector<size_t>(1, idx));
                    } else {
                        itr->second.push_back(idx);
                    }
                } else {
                    throw std::runtime_error("wrong type of field");
                }
                    
            }
            for (auto& right_record: right) {
                if(right_record[right_col].data_idx == 0xFFFF) continue;
                auto key = ((int32_t(right_record[right_col].column_idx) & 0xFFFF) << 16) |
                                (int32_t(right_record[right_col].table_idx)  & 0xFFFF);

                if (key >= 0) {
                    if (auto itr = hash_table.find(key); itr != hash_table.end()) {
                        for (auto left_idx: itr->second) {
                            auto&             left_record = left[left_idx];
                            std::vector<value_t> new_record;
                            new_record.reserve(output_attrs.size());
                            for (auto [col_idx, _]: output_attrs) {
                                if (col_idx < left_record.size()) {
                                    new_record.emplace_back(left_record[col_idx]);
                                } else {
                                    new_record.emplace_back(
                                        right_record[col_idx - left_record.size()]);
                                }
                            }
                            results.emplace_back(std::move(new_record));
                        }
                    }
                } else {
                    throw std::runtime_error("wrong type of field");
                }      
            }
        } else {
            for (auto&& [idx, record]: right | views::enumerate) {
                if(record[right_col].data_idx == 0xFFFF) continue;
                auto key = ((int32_t(record[right_col].column_idx) & 0xFFFF) << 16) |
                                (int32_t(record[right_col].table_idx)  & 0xFFFF);

                if (key >= 0) {
                    if (auto itr = hash_table.find(key); itr == hash_table.end()) {
                        hash_table.emplace(key, std::vector<size_t>(1, idx));
                    } else {
                        itr->second.push_back(idx);
                    }
                } else {
                    throw std::runtime_error("wrong type of field");
                }
            }
            for (auto& left_record: left) {
                if(left_record[left_col].data_idx == 0xFFFF) continue;
                auto key = ((int32_t(left_record[left_col].column_idx) & 0xFFFF) << 16) |
                                (int32_t(left_record[left_col].table_idx)  & 0xFFFF);
                if (key >= 0) {
                    if (auto itr = hash_table.find(key); itr != hash_table.end()) {
                        for (auto right_idx: itr->second) {
                            auto&             right_record = right[right_idx];
                            std::vector<value_t> new_record;
                            new_record.reserve(output_attrs.size());
                            for (auto [col_idx, _]: output_attrs) {
                                if (col_idx < left_record.size()) {
                                    new_record.emplace_back(left_record[col_idx]);
                                } else {
                                    new_record.emplace_back(
                                        right_record[col_idx - left_record.size()]);
                                }
                            }
                            results.emplace_back(std::move(new_record));
                        }
                    }
                } else {
                    throw std::runtime_error("wrong type of field");
                }
                    
            }
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
    std::vector<std::vector<value_t>> results;

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
    return scan_table(input, output_attrs, table_id); 
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
    auto materialized_table = materialize_table(ret, plan);     
    Table table{std::move(materialized_table), std::move(ret_types)};
    return table.to_columnar(); //Should remove after join returns columnar table instead of row
}

void* build_context() {
    return nullptr;
}

void destroy_context([[maybe_unused]] void* context) {}

} // namespace Contest