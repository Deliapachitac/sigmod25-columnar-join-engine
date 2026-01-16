#include <hardware.h>
#include <plan.h>
#include <table.h>
#include <iostream>
#include <robin_hood.h>
#include <table_reader.h>
#include <cuckoo_hash.h>
#include <hopscotch.h>
#include <cstdlib>
#include <algorithm>
#include <unchained_hashtable.h>
#include <thread>

using HashTable = unchained_ht;

namespace Contest
{

    using ExecuteResult = std::vector<column_t>;

    ExecuteResult execute_impl(const Plan &plan, size_t node_idx);

    struct JoinAlgorithm
    {
        bool build_left;
        ExecuteResult &left;
        ExecuteResult &right;
        ExecuteResult &results;
        size_t left_col, right_col;
        const std::vector<std::tuple<size_t, DataType>> &output_attrs;

        auto run()
        {
            HashTable hash_table;

            if (build_left)
            {
                extract_keys_from_column_parallel(hash_table, left[left_col]);
            }
            else
            {
                extract_keys_from_column_parallel(hash_table, right[right_col]);
            }

            // 1. MUST happen on the main thread after extraction threads are joined
            if (!hash_table.prepare_build())
                return;
            // 2. Spawn workers.
            std::vector<std::thread> threads;
            threads.reserve(8);
            for (size_t i = 0; i < 8; ++i)
            {
                // Pass pointers/values explicitly to ensure thread safety
                threads.emplace_back(&unchained_ht::post_process_build, &hash_table, i, i);
            }

            for (auto &t : threads)
                t.join();
            //for(int p = 0; p < 8; p++) hash_table.post_process_build(0, p);
            // 3. Finalize and Probe
            hash_table.finalize_build();
            probe_phase(hash_table, left, right, build_left);
        }

    private:
        void extract_keys_from_column_parallel(HashTable &hash_table, column_t &column)
        {
            /* Precompute page row offsets (single-threaded, safe) */
            std::vector<size_t> page_offsets(column.pages.size());

            /*  Compute starting row index for each page */
            size_t total = 0;
            for (size_t i = 0; i < column.pages.size(); ++i)
            {
                page_offsets[i] = total;
                total += *reinterpret_cast<uint16_t *>(column.pages[i]->data);
            }
            const size_t num_threads = 8;
            const size_t num_pages = column.pages.size();

            /* Launch threads to process pages in parallel */
            std::vector<std::thread> threads;

            threads.reserve(num_threads);

            for (size_t tid = 0; tid < num_threads; ++tid)
            {
                threads.emplace_back([&, tid]()
                                     {

            /* Each thread processes pages in round-robin fashion */
            for (size_t page_idx = tid; page_idx < num_pages; page_idx += num_threads) {

                auto* page = column.pages[page_idx];

                uint16_t num_rows =
                    *reinterpret_cast<uint16_t*>(page->data);

                auto* buffer =
                    reinterpret_cast<value_t*>(page->data + sizeof(uint16_t));

                /* Compute base row index for this page */
                size_t row_idx = page_offsets[page_idx];

                for (uint16_t i = 0; i < num_rows; i++) {

                    const value_t& record = buffer[i];

                    if (record.data_idx == 0xFFFF) {
                        row_idx++;
                        continue;
                    }

                    int32_t key =
                        ((int32_t(record.column_idx) & 0xFFFF) << 16) |
                        (int32_t(record.table_idx)  & 0xFFFF);

                    if (key >= 0) {
                        hash_table.build_insert(key, row_idx, tid);
                    } else {
                        throw std::runtime_error("wrong type of field");
                    }

                    row_idx++;
                }
            } });
            }

            for (auto &t : threads)
                t.join();
        }

        std::vector<size_t> build_prefix(const column_t &col)
        {
            // We create a vector that will contain for every page the starting row index
            std::vector<size_t> prefix;
            prefix.reserve(col.pages.size());

            // The starting index of the first page is always 0
            size_t total = 0;
            for (auto *page : col.pages)
            {
                prefix.push_back(total);
                total += *reinterpret_cast<uint16_t *>(page->data); // every time a new page starts we add the number of rows from the previous page
            }
            return prefix;
        }

        value_t get_value(const column_t &col, const std::vector<size_t> &prefix, size_t row_idx)
        {

            // Now when we want to get a value_t entry from a specific row index
            // we can use the prefix to find the page

            // The upper_bound finds the first element in prefix that is greater than row_idx
            // and it returns an iterator to that element
            auto it = std::upper_bound(prefix.begin(), prefix.end(), row_idx);

            // The page index is the index of the element before the found element
            // because that page contains the row we are looking for
            size_t page_idx = (it - prefix.begin()) - 1;

            // Calculate the index in the page by subtracting the starting index of the page from the row index
            // For example if the row index is in 275 and the page starts at 250 the local index is 25
            size_t local_idx = row_idx - prefix[page_idx];

            // Get the pointer to the page and where the value_t entries begin
            //  and return the value_t entry at the  index we calculated earlier
            auto *page = col.pages[page_idx];
            auto *buf = reinterpret_cast<value_t *>(page->data + sizeof(uint16_t));
            return buf[local_idx];
        }

        void probe_phase(HashTable &hash_table, ExecuteResult &left, ExecuteResult &right, bool is_left)
        {

            // Create and initialise the output column_t with the data type from the vector output_attrs
            if (results.empty())
            {
                for (auto [col_idx, dtype] : output_attrs)
                {
                    results.emplace_back(dtype);
                }
            }

            // We create a 2D vector wchich will contain prefixes for each table (left and right)
            //  The prefixes shows the starting row index for each page
            // For example if we have 3 pages with 100, 150, 200 rows the prefix will be [0,100,250]
            // If we need to find row index for row 180 we can see that it is in page 2 because 180 > 100 and 180 < 250
            std::vector<std::vector<size_t>> left_prefixes(left.size());
            std::vector<std::vector<size_t>> right_prefixes(right.size());

            // For each column we build the prefix sum which will help us find the page and the   row index in the inserting phase
            for (size_t i = 0; i < left.size(); ++i)
            {
                left_prefixes[i] = build_prefix(left[i]);
            }
            for (size_t i = 0; i < right.size(); ++i)
            {
                right_prefixes[i] = build_prefix(right[i]);
            }

            column_t &probe_col = is_left ? right[right_col] : left[left_col];
            size_t probe_row_idx = 0;

            // For each page of the second column (the probe  side)we excract the number of rows and then the value_t entries . We save save the references to variables
            for (auto *page : probe_col.pages)
            {
                uint16_t num_rows = *reinterpret_cast<uint16_t *>(page->data);
                auto *buffer = reinterpret_cast<value_t *>(page->data + sizeof(uint16_t));

                for (uint16_t i = 0; i < num_rows; i++)
                {
                    const value_t &record = buffer[i];
                    // If the value is null we skip it
                    if (record.data_idx == 0xFFFF)
                    {
                        probe_row_idx++;
                        continue;
                    }

                    int32_t key = ((int32_t(record.column_idx) & 0xFFFF) << 16) | (int32_t(record.table_idx) & 0xFFFF);

                    // After getting the key we check if it exists in the hash table or not, and get the vector of matching row indexes
                    auto matches = hash_table.probe(key);

                    // The key here exists so we need to iterate through all matching rows from the build side (the table we extracter with the previous function)
                    if (!matches.empty())
                    {
                        for (auto build_row_idx : matches)
                        {

                            // For each column of the table (left and right) we need to get the value_t entry and insert it into the output column_t
                            for (size_t out_col_idx = 0; out_col_idx < output_attrs.size(); ++out_col_idx)
                            {
                                size_t src_col_idx = std::get<0>(output_attrs[out_col_idx]);
                                value_t value_to_insert;

                                // If we are building from the left table and probing from the right table
                                if (is_left)
                                {

                                    // If src_col_idx is in range [0,left.size] we get the value from the left table
                                    if (src_col_idx < left.size())
                                    {
                                        value_to_insert = get_value(left[src_col_idx], left_prefixes[src_col_idx], build_row_idx);
                                        // Else  we get the value from the right table
                                    }
                                    else
                                    {
                                        value_to_insert = get_value(right[src_col_idx - left.size()], right_prefixes[src_col_idx - left.size()], probe_row_idx);
                                    }

                                    // If we are building from the right table and probing from the left table
                                }
                                else
                                {

                                    // If src_col_idx is in range [0,left.size] we get the value from the left table
                                    if (src_col_idx < left.size())
                                    {
                                        value_to_insert = get_value(left[src_col_idx], left_prefixes[src_col_idx], probe_row_idx);

                                        // Else  we get the value from the right table
                                    }
                                    else
                                    {
                                        value_to_insert = get_value(right[src_col_idx - left.size()], right_prefixes[src_col_idx - left.size()], build_row_idx);
                                    }
                                }

                                results[out_col_idx].insert_value_to_page(value_to_insert);
                            }
                            results[0].num_rows++;
                        }
                    }
                    probe_row_idx++;
                }
            }

            // We also need to set the num_rows for all the output columns
            for (size_t i = 1; i < results.size(); ++i)
            {
                results[i].num_rows = results[0].num_rows;
            }
        }
    };

    ExecuteResult execute_hash_join(const Plan &plan,
                                    const JoinNode &join,
                                    const std::vector<std::tuple<size_t, DataType>> &output_attrs)
    {
        auto left_idx = join.left;
        auto right_idx = join.right;
        auto &left_node = plan.nodes[left_idx];
        auto &right_node = plan.nodes[right_idx];
        auto &left_types = left_node.output_attrs;
        auto &right_types = right_node.output_attrs;
        auto left = execute_impl(plan, left_idx);
        auto right = execute_impl(plan, right_idx);
        std::vector<column_t> results;

        JoinAlgorithm join_algorithm{.build_left = join.build_left,
                                     .left = left,
                                     .right = right,
                                     .results = results,
                                     .left_col = join.left_attr,
                                     .right_col = join.right_attr,
                                     .output_attrs = output_attrs};

        join_algorithm.run();

        return results;
    }

    ExecuteResult execute_scan(const Plan &plan,
                               const ScanNode &scan,
                               const std::vector<std::tuple<size_t, DataType>> &output_attrs)
    {
        auto table_id = scan.base_table_id;
        auto &input = plan.inputs[table_id];
        return scan_column_table(input, output_attrs, table_id);
    }

    ExecuteResult execute_impl(const Plan &plan, size_t node_idx)
    {
        auto &node = plan.nodes[node_idx];
        return std::visit(
            [&](const auto &value)
            {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, JoinNode>)
                {
                    return execute_hash_join(plan, value, node.output_attrs);
                }
                else
                {
                    return execute_scan(plan, value, node.output_attrs);
                }
            },
            node.data);
    }

    ColumnarTable execute(const Plan &plan, [[maybe_unused]] void *context)
    {
        namespace views = ranges::views;
        auto ret = execute_impl(plan, plan.root);
        auto ret_types = plan.nodes[plan.root].output_attrs | views::transform([](const auto &v)
                                                                               { return std::get<1>(v); }) |
                         ranges::to<std::vector<DataType>>();
        return convert_column_t_to_columnar(ret, plan, ret_types);
    }
    void *build_context()
    {
        return nullptr;
    }

    void destroy_context([[maybe_unused]] void *context) {}

} // namespace Contest