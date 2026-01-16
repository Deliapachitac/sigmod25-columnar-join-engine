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
#include <chrono>
#include <thread>

#define TIMING
#undef TIMING
using HashTable = unchained_ht;
namespace Contest
{

    using ExecuteResult = std::vector<column_t>;
#ifdef TIMING
    struct Timer
    {
        std::chrono::time_point<std::chrono::high_resolution_clock> start;
        std::string tag;

        Timer(std::string task) : tag(task)
        {
            start = std::chrono::high_resolution_clock::now();
        }

        ~Timer()
        {
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> duration = end - start;
            std::cout << tag << " took: " << duration.count() * 1000.0f << "ms" << std::endl;
        }
    };
#endif
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
            namespace views = ranges::views;

            HashTable hash_table;

            if (build_left)
            {
                {
#ifdef TIMING
                    Timer time{"Extract keys"};
#endif
                    // Extract keys from left table and add to hash table
                    extract_keys_from_column(hash_table, left[left_col]);
                }
                {
#ifdef TIMING
                    Timer time{"Finalize build"};
#endif
                    hash_table.finalize_build();
                }
                {
#ifdef TIMING
                    Timer time{"Probe"};
#endif
                    // in the probe phase we use the hash table to find matches from the right table
                    probe_phase(hash_table, left, right, true);
                }
            }
            else
            {
                {
#ifdef TIMING
                    Timer time{"Extract keys"};
#endif
                    // Extract keys from right table and add to hash table
                    extract_keys_from_column(hash_table, right[right_col]);
                }
                {
#ifdef TIMING
                    Timer time{"Finalize build"};
#endif
                    hash_table.finalize_build();
                }

                // in the probe phase we use the hash table to find matches from the right table
                {
#ifdef TIMING
                    Timer time{"Probe"};
#endif
                    probe_phase(hash_table, left, right, false);
                }
            }
        }

    private:
        void extract_keys_from_column(HashTable &hash_table, column_t &column)
        {

            // for every page in the column we exctract the key and insert it into the hash table
            size_t row_idx = 0;

            // If the column contains int32 entries with null values so we are maintaining the same logic as before
            if (column.valid)
            {
                for (auto *page : column.pages)
                {

                    // The page contains a number of rows and then the value_t entries . We save save the references to variables
                    uint16_t num_rows = *reinterpret_cast<uint16_t *>(page->data);
                    auto *buffer = reinterpret_cast<value_t *>(page->data + sizeof(uint16_t));

                    // for each value_t entry wecheck  if it is null or int and insert in the hash table
                    for (uint16_t i = 0; i < num_rows; i++)
                    {
                        const value_t &record = buffer[i];
                        if (record.data_idx == 0xFFFF)
                        {
                            row_idx++;
                            continue;
                        }

                        int32_t key = ((int32_t(record.column_idx) & 0xFFFF) << 16) | (int32_t(record.table_idx) & 0xFFFF);

                        // If key already exists  we insert the row index in the vector of the row indexes
                        // else we create a new entry
                        if (key >= 0)
                        {
                            hash_table.build_insert(key, row_idx);
                            row_idx++;
                        }
                        else
                        {
                            throw std::runtime_error("wrong type of field");
                        }
                    }
                }
            }
            else
            {
                // if the column contains only int32 entries without null values
                for (auto *page : column.pages)
                {

                    // The page contains a number of rows and then the int32_t entries . We save save the references to variables
                    auto num_rows = *reinterpret_cast<uint16_t *>(page->data);
                    auto *buffer = reinterpret_cast<int32_t *>(page->data + 4);

                    for (uint16_t i = 0; i < num_rows; i++)
                    {
                        int32_t key = buffer[i];
                        hash_table.build_insert(key, row_idx);
                        row_idx++;
                    }
                }
            }
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

            // If the column contains int32 entries with null values so we are maintaining the same logic as before
            if (col.valid)
            {
                auto *buf = reinterpret_cast<value_t *>(page->data + sizeof(uint16_t));
                return buf[local_idx];
            }
            else
            {
                // This column contains only int32 entries without null values
                auto *data_begin = reinterpret_cast<int32_t *>(page->data + 4);
                int32_t value = data_begin[local_idx];
                return value_t{
                    .table_idx = static_cast<uint16_t>(value & 0xFFFF),
                    .column_idx = static_cast<uint16_t>((value >> 16) & 0xFFFF),
                    .page_idx = 0,
                    .data_idx = 0xFFFE // INT_VALUE marker
                };
            }
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

            std::vector<std::thread> threads;
            
            size_t threadNum = std::thread::hardware_concurrency();

            if(threadNum == 0){
                threadNum = 8;
            }
            // Only allocate memory for the temporary thread page vector if we are to use it
            for (auto& col : results) {
                col.prepare_for_threads(threadNum);
            }   
            // We collect the row count individually to avoid a race condition and then we add them all together
            size_t row_counts[threadNum];
            threads.reserve(threadNum);
            for(size_t tid = 0; tid < threadNum; ++tid)
            {
                threads.emplace_back([&, tid]()
                {
            row_counts[tid] = 0;
            // We use a round-robin approach to the thread work                                
            for (int page_idx = tid; page_idx < probe_col.pages.size(); page_idx += threadNum)
            {
                // For each page of the second column (the probe  side) we extract the number of rows and then the value_t entries . We save save the references to variables
                auto* page = probe_col.pages[page_idx];
                uint16_t num_rows = *reinterpret_cast<uint16_t *>(page->data);

                value_t *value_buffer = nullptr;
                int32_t *int_buffer = nullptr;
                
                // If the col contains int32 entries with null values   we are maintaining the same logic as before
                if (probe_col.valid)
                {
                    value_buffer = reinterpret_cast<value_t *>(page->data + sizeof(uint16_t));
                }
                else
                {
                    // This column contains only int32 entries without null values we directly get the int32 buffer
                    int_buffer = reinterpret_cast<int32_t *>(page->data + 4);
                }
                const std::vector<size_t>& current_col_prefixes = is_left ? right_prefixes[right_col] : left_prefixes[left_col];
                
                for (uint16_t i = 0; i < num_rows; i++)
                {
                    int32_t key;
                    size_t local_probe_row_idx = current_col_prefixes[page_idx] + i; 
                    if (probe_col.valid)
                    {
                        const value_t &record = value_buffer[i];
                        // If the value is null we skip it
                        if (record.data_idx == 0xFFFF)
                        {
                            local_probe_row_idx++;
                            continue;
                        }

                        key = ((int32_t(record.column_idx) & 0xFFFF) << 16) | (int32_t(record.table_idx) & 0xFFFF);
                    }
                    else
                    {
                        key = int_buffer[i];
                    }

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
                                        value_to_insert = get_value(right[src_col_idx - left.size()], right_prefixes[src_col_idx - left.size()], local_probe_row_idx);
                                    }

                                    // If we are building from the right table and probing from the left table
                                }
                                else
                                {

                                    // If src_col_idx is in range [0,left.size] we get the value from the left table
                                    if (src_col_idx < left.size())
                                    {
                                        value_to_insert = get_value(left[src_col_idx], left_prefixes[src_col_idx], local_probe_row_idx);

                                        // Else  we get the value from the right table
                                    }
                                    else
                                    {
                                        value_to_insert = get_value(right[src_col_idx - left.size()], right_prefixes[src_col_idx - left.size()], build_row_idx);
                                    }
                                }

                                results[out_col_idx].insert_value_to_page(value_to_insert, tid);
                            }
                            row_counts[tid]++;
                        }
                    }
                    local_probe_row_idx++;
                }
            }});
        }   
            size_t total_rows = 0;
            for (size_t tid = 0; tid < threadNum; tid++){
                threads[tid].join();
                total_rows += row_counts[tid];
            }
                
            // We also need to set the num_rows for all the output columns
            for (size_t i = 0; i < results.size(); ++i)
            {
                results[i].collect_vectors();
                results[i].num_rows = total_rows;
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
#ifdef TIMING
        Timer time{"Scan"};
#endif
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
#ifdef TIMING
        Timer time{"Convert from column to columnar"};
#endif
        return convert_column_t_to_columnar(ret, plan, ret_types);
    }
    void *build_context()
    {
        return nullptr;
    }

    void destroy_context([[maybe_unused]] void *context) {}

} // namespace Contest