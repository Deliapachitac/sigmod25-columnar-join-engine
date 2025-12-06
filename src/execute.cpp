#include <hardware.h>
#include <plan.h>
#include <table.h>
#include <iostream>
#include <robin_hood.h>
#include <table_reader.h>
#include <cuckoo_hash.h>
#include <hopscotch.h>
#include <cstdlib>
#include <unchained_hashtable.h>

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

        /* -------- SELECT BUILD / PROBE SIDES -------- */
        auto &build_side  = build_left ? left  : right;
        auto &probe_side  = build_left ? right : left;
        auto  build_col   = build_left ? left_col  : right_col;
        auto  probe_col   = build_left ? right_col : left_col;

        /* ---------- BUILD UNCHAINED HASH TABLE ---------- */
        unchained_ht ht(build_side.size());

        for (auto&& [idx, record] : build_side | views::enumerate) {

            if (record[build_col].data_idx == 0xFFFF)
                continue;

            int32_t key =
                ((int32_t(record[build_col].column_idx) & 0xFFFF) << 16) |
                 (int32_t(record[build_col].table_idx ) & 0xFFFF);

            // Store build index as payload
            ht.build_insert(key, idx);
        }

        ht.finalize_build();

        /* ------------------- PROBE ------------------- */
        for (auto &probe_record : probe_side) {

            if (probe_record[probe_col].data_idx == 0xFFFF)
                continue;

            int32_t key =
                ((int32_t(probe_record[probe_col].column_idx) & 0xFFFF) << 16) |
                 (int32_t(probe_record[probe_col].table_idx ) & 0xFFFF);

            auto [begin, end] = ht.lookup(key);

            if (!begin)          // Bloom filter or no match
                continue;

            /* Iterate all matches (indices from build side) */
            for (auto p = begin; p != end; ++p) {

                size_t build_idx = p->value;
                auto &build_record = build_side[build_idx];

                std::vector<value_t> new_record;
                new_record.reserve(output_attrs.size());

                auto &left_record  = build_left ? build_record : probe_record;
                auto &right_record = build_left ? probe_record  : build_record;

                for (auto [col_idx, _] : output_attrs) {
                    if (col_idx < left_record.size())
                        new_record.emplace_back(left_record[col_idx]);
                    else
                        new_record.emplace_back(
                            right_record[col_idx - left_record.size()]);
                }

                results.emplace_back(std::move(new_record));
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
    return materialize_columnar_table(ret, plan, ret_types); 
}
void* build_context() {
    return nullptr;
}

void destroy_context([[maybe_unused]] void* context) {}

} // namespace Contest