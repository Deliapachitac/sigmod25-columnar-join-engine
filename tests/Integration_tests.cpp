#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <table.h>
#include <plan.h>
#include <dependencies.h>

bool is_null(const Data &d) { return std::holds_alternative<std::monostate>(d); }
std::vector<std::vector<Data>> generate_random_rows(size_t num_rows, const std::vector<DataType> &schema)
{
    std::vector<std::vector<Data>> rows;
    rows.reserve(num_rows);

    // Random Number Generator Setup
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int32_t> int_dist(0, 100); // Random Ints 0-100
    std::uniform_int_distribution<int> char_dist('a', 'z');  // Random Chars a-z
    std::uniform_int_distribution<int> len_dist(3, 10);      // Random String Length 3-10
    std::bernoulli_distribution null_dist(0.1);              // 10% Chance of NULL

    for (size_t i = 0; i < num_rows; ++i)
    {
        std::vector<Data> row;
        row.reserve(schema.size());

        for (DataType type : schema)
        {
            // Optional: 10% chance to insert NULL (std::monostate)
            if (null_dist(gen))
            {
                row.push_back(std::monostate{});
                continue;
            }

            switch (type)
            {
            case DataType::INT32:
            {
                row.push_back(int_dist(gen));
                break;
            }
            case DataType::VARCHAR:
            {
                int len = len_dist(gen);
                std::string s;
                s.reserve(len);
                for (int c = 0; c < len; ++c)
                    s.push_back((char)char_dist(gen));
                row.push_back(s);
                break;
            }
            default:
                row.push_back(std::monostate{});
                break;
            }
        }
        rows.push_back(std::move(row));
    }

    return rows;
}
std::vector<std::vector<Data>> generate_random_rows_with_long_string(
    size_t num_rows,
    const std::vector<DataType> &schema)
{
    std::vector<std::vector<Data>> rows;
    rows.reserve(num_rows);

    // Random Number Generator Setup
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int32_t> int_dist(0, 100);       // Random Ints 0-100
    std::uniform_int_distribution<int> char_dist('a', 'z');        // Random Chars a-z
    std::uniform_int_distribution<int> len_dist(8192, 16000);       
    std::bernoulli_distribution null_dist(0.1);                    // 10% chance of NULL

    for (size_t i = 0; i < num_rows; ++i)
    {
        std::vector<Data> row;
        row.reserve(schema.size());

        for (DataType type : schema)
        {
            if (null_dist(gen))
            {
                row.push_back(std::monostate{});
                continue;
            }

            switch (type)
            {
                case DataType::INT32:
                {
                    row.push_back(int_dist(gen));
                    break;
                }
                case DataType::VARCHAR:
                {
                    int len = len_dist(gen);
                    std::string s;
                    s.reserve(len);

                    for (int c = 0; c < len; ++c)
                        s.push_back(static_cast<char>(char_dist(gen)));

                    row.push_back(std::move(s));
                    break;
                }
                default:
                    row.push_back(std::monostate{});
                    break;
            }
        }
        rows.push_back(std::move(row));
    }

    return rows;
}
void print_variant(const Data &d)
{
    if (std::holds_alternative<std::monostate>(d))
    {
        std::cerr << "NULL, ";
    }
    else if (std::holds_alternative<int32_t>(d))
    {
        std::cerr << std::get<int32_t>(d) << ", ";
    }
    else if (std::holds_alternative<std::string>(d))
    {
        std::cerr << "\"" << std::get<std::string>(d) << "\", ";
    }
}
void assert_tables_equal(const ColumnarTable &actual, const ColumnarTable &expected)
{

    // ---------------------------------------------------------
    // CHECK 1: METADATA & SCHEMA (Fail Fast)
    // ---------------------------------------------------------

    // 1. Row Count
    INFO("Row count mismatch");
    REQUIRE(actual.num_rows == expected.num_rows);

    // 2. Column Count
    INFO("Column count mismatch");
    REQUIRE(actual.columns.size() == expected.columns.size());

    // 3. Column Types
    for (size_t i = 0; i < actual.columns.size(); ++i)
    {
        INFO("Column type mismatch at index " << i);
        REQUIRE(actual.columns[i].type == expected.columns[i].type);
    }

    // ---------------------------------------------------------
    // CHECK 2: DATA NORMALIZATION
    // ---------------------------------------------------------

    std::vector<std::vector<Data>> actual_rows = Table::from_columnar(actual).table();
    std::vector<std::vector<Data>> expected_rows = Table::from_columnar(expected).table();

    // ---------------------------------------------------------
    // CHECK 3: CANONICAL SORT
    // ---------------------------------------------------------

    std::sort(actual_rows.begin(), actual_rows.end());
    std::sort(expected_rows.begin(), expected_rows.end());

    // ---------------------------------------------------------
    // CHECK 4: DEEP CONTENT EQUALITY
    // ---------------------------------------------------------

    // We iterate to find the specific row that differs for better debugging
    bool mismatch_found = false;
    size_t error_limit = 5; // Don't spam console if 10k rows fail
    size_t errors = 0;

    for (size_t i = 0; i < actual_rows.size(); ++i)
    {
        if (actual_rows[i] != expected_rows[i])
        {
            mismatch_found = true;
            errors++;

            // Print detailed error for the first few failures
            if (errors <= error_limit)
            {
                std::cerr << "Mismatch at sorted row [" << i << "]:\n";

                std::cerr << "  Actual:   [";
                for (const auto &c : actual_rows[i])
                    print_variant(c);
                std::cerr << "]\n";

                std::cerr << "  Expected: [";
                for (const auto &c : expected_rows[i])
                    print_variant(c);
                std::cerr << "]\n";
            }
        }
    }

    if (mismatch_found)
    {
        FAIL("Table content mismatch! See stderr for details.");
    }

    REQUIRE(actual_rows == expected_rows);
}
TEST_CASE("Simple join", "[single_join]")
{
    Plan plan;
    plan.new_scan_node(0, {{0, DataType::INT32}, {2, DataType::VARCHAR}});
    plan.new_scan_node(1, {{0, DataType::INT32}, {3, DataType::VARCHAR}});
    plan.new_join_node(true, 0, 1, 0, 0, {{0, DataType::INT32}, {1, DataType::VARCHAR}, {3, DataType::VARCHAR}});
    std::vector<DataType> types1 = {DataType::INT32, DataType::VARCHAR, DataType::VARCHAR, DataType::INT32};
    std::vector<DataType> types2 = {DataType::INT32, DataType::VARCHAR, DataType::INT32, DataType::VARCHAR};
    std::vector<std::vector<Data>> data1 = generate_random_rows(100, types1);
    std::vector<std::vector<Data>> data2 = generate_random_rows(100, types2);
    Table table1(std::move(data1), std::move(types1));
    Table table2(std::move(data2), std::move(types2));
    ColumnarTable input1 = table1.to_columnar();
    ColumnarTable input2 = table2.to_columnar();
    plan.inputs.emplace_back(std::move(input1));
    plan.inputs.emplace_back(std::move(input2));
    plan.root = 2;
    auto *context = Contest::build_context();
    auto result = Contest::execute(plan, context);
    Contest::destroy_context(context);
    auto *ref_ctx = Reference::build_context();
    auto ref_result = Reference::execute(plan, ref_ctx);
    Reference::destroy_context(ref_ctx);
    assert_tables_equal(result, ref_result);
}
TEST_CASE("Simple join right", "[single_join_right]")
{
    Plan plan;
    plan.new_scan_node(0, {{0, DataType::INT32}, {2, DataType::VARCHAR}});
    plan.new_scan_node(1, {{0, DataType::INT32}, {3, DataType::VARCHAR}});
    plan.new_join_node(false, 0, 1, 0, 0, {{0, DataType::INT32}, {1, DataType::VARCHAR}, {3, DataType::VARCHAR}});
    std::vector<DataType> types1 = {DataType::INT32, DataType::VARCHAR, DataType::VARCHAR, DataType::INT32};
    std::vector<DataType> types2 = {DataType::INT32, DataType::VARCHAR, DataType::INT32, DataType::VARCHAR};
    std::vector<std::vector<Data>> data1 = generate_random_rows(100, types1);
    std::vector<std::vector<Data>> data2 = generate_random_rows(100, types2);
    Table table1(std::move(data1), std::move(types1));
    Table table2(std::move(data2), std::move(types2));
    ColumnarTable input1 = table1.to_columnar();
    ColumnarTable input2 = table2.to_columnar();
    plan.inputs.emplace_back(std::move(input1));
    plan.inputs.emplace_back(std::move(input2));
    plan.root = 2;
    auto *context = Contest::build_context();
    auto result = Contest::execute(plan, context);
    Contest::destroy_context(context);
    auto *ref_ctx = Reference::build_context();
    auto ref_result = Reference::execute(plan, ref_ctx);
    Reference::destroy_context(ref_ctx);
    assert_tables_equal(result, ref_result);
}

TEST_CASE("Bushy Join", "[bushy_join]")
{
    Plan plan;

    // Scan nodes with richer schemas
    plan.new_scan_node(0, {
                              {0, DataType::INT32},   // id
                              {1, DataType::VARCHAR}, // region (Changed from 2)
                              {2, DataType::VARCHAR}  // country (Changed from 3)
                          });

    // Scan Node 1 (Table 2 has 3 cols: 0, 1, 2)
    plan.new_scan_node(1, {
                              {0, DataType::INT32},   // id
                              {1, DataType::VARCHAR}, // category (Changed from 3)
                              {2, DataType::VARCHAR}  // subcategory (Changed from 4)
                          });

    // Scan Node 2 (Table 3 has 3 cols: 0, 1, 2)
    plan.new_scan_node(2, {
                              {0, DataType::INT32},   // id
                              {1, DataType::VARCHAR}, // supplier (Changed from 2)
                              {2, DataType::INT32}    // rating (Changed from 1, or keep 1/2 depending on schema order)
                          });
    // Note: types3 is {INT, VARCHAR, INT}, so index 1 is VARCHAR, index 2 is INT.

    // Scan Node 3 (Table 4 has 3 cols: 0, 1, 2)
    plan.new_scan_node(3, {
                              {0, DataType::INT32},   // id
                              {1, DataType::VARCHAR}, // status (Changed from 3)
                              {2, DataType::INT32}    // priority (Changed from 1)
                          });

    // JOIN 0–1 → columns: id, region, country, category, subcategory
    plan.new_join_node(true, 0, 1, 0, 0,
                       {
                           {0, DataType::INT32},   // id
                           {1, DataType::VARCHAR}, // region
                           {2, DataType::VARCHAR}, // country
                           {4, DataType::VARCHAR}, // category
                           {5, DataType::VARCHAR}  // subcategory
                       });

    // JOIN 2–3 → columns: id, supplier, rating, status, priority
    plan.new_join_node(true, 2, 3, 0, 0,
                       {
                           {0, DataType::INT32},   // id
                           {1, DataType::VARCHAR}, // supplier
                           {2, DataType::INT32},   // rating
                           {4, DataType::VARCHAR}, // status
                           {5, DataType::INT32}    // priority
                       });

    // Final join (4–5)
    // Output columns: all from left then all from right
    plan.new_join_node(true, 4, 5, 0, 0,
                       {
                           // From left (0–1 join)
                           {0, DataType::INT32},   // id
                           {1, DataType::VARCHAR}, // region
                           {2, DataType::VARCHAR}, // country
                           {3, DataType::VARCHAR}, // category
                           {4, DataType::VARCHAR}, // subcategory

                           // From right (2–3 join)
                           {6, DataType::VARCHAR}, // supplier
                           {7, DataType::INT32},   // rating
                           {8, DataType::VARCHAR}, // status
                           {9, DataType::INT32}    // priority
                       });

    // Table type definitions
    std::vector<DataType> types1 = {DataType::INT32, DataType::VARCHAR, DataType::VARCHAR};
    std::vector<DataType> types2 = {DataType::INT32, DataType::VARCHAR, DataType::VARCHAR};
    std::vector<DataType> types3 = {DataType::INT32, DataType::VARCHAR, DataType::INT32};
    std::vector<DataType> types4 = {DataType::INT32, DataType::VARCHAR, DataType::INT32};

    // Generate data
    auto data1 = generate_random_rows_with_long_string(100, types1);
    auto data2 = generate_random_rows_with_long_string(100, types2);
    auto data3 = generate_random_rows_with_long_string(100, types3);
    auto data4 = generate_random_rows_with_long_string(100, types4);

    Table table1(std::move(data1), types1);
    Table table2(std::move(data2), types2);
    Table table3(std::move(data3), types3);
    Table table4(std::move(data4), types4);

    plan.inputs.emplace_back(table1.to_columnar());
    plan.inputs.emplace_back(table2.to_columnar());
    plan.inputs.emplace_back(table3.to_columnar());
    plan.inputs.emplace_back(table4.to_columnar());

    plan.root = 6;

    auto *context = Contest::build_context();
    auto result = Contest::execute(plan, context);
    Contest::destroy_context(context);

    auto *ref_ctx = Reference::build_context();
    auto ref_result = Reference::execute(plan, ref_ctx);
    Reference::destroy_context(ref_ctx);

    assert_tables_equal(result, ref_result);
}

TEST_CASE("Bushy Join with Long string", "[bushy_join_lstring]")
{
    Plan plan;

    // Scan nodes with richer schemas
    // Scan nodes with richer schemas
    plan.new_scan_node(0, {
                              {0, DataType::INT32},   // id
                              {1, DataType::VARCHAR}, // region (Changed from 2)
                              {2, DataType::VARCHAR}  // country (Changed from 3)
                          });

    // Scan Node 1 (Table 2 has 3 cols: 0, 1, 2)
    plan.new_scan_node(1, {
                              {0, DataType::INT32},   // id
                              {1, DataType::VARCHAR}, // category (Changed from 3)
                              {2, DataType::VARCHAR}  // subcategory (Changed from 4)
                          });

    // Scan Node 2 (Table 3 has 3 cols: 0, 1, 2)
    plan.new_scan_node(2, {
                              {0, DataType::INT32},   // id
                              {1, DataType::VARCHAR}, // supplier (Changed from 2)
                              {2, DataType::INT32}    // rating (Changed from 1, or keep 1/2 depending on schema order)
                          });
    // Note: types3 is {INT, VARCHAR, INT}, so index 1 is VARCHAR, index 2 is INT.

    // Scan Node 3 (Table 4 has 3 cols: 0, 1, 2)
    plan.new_scan_node(3, {
                              {0, DataType::INT32},   // id
                              {1, DataType::VARCHAR}, // status (Changed from 3)
                              {2, DataType::INT32}    // priority (Changed from 1)
                          });

    // JOIN 0–1 → columns: id, region, country, category, subcategory
    plan.new_join_node(true, 0, 1, 0, 0,
                       {
                           {0, DataType::INT32},   // id
                           {1, DataType::VARCHAR}, // region
                           {2, DataType::VARCHAR}, // country
                           {4, DataType::VARCHAR}, // category
                           {5, DataType::VARCHAR}  // subcategory
                       });

    // JOIN 2–3 → columns: id, supplier, rating, status, priority
    plan.new_join_node(true, 2, 3, 0, 0,
                       {
                           {0, DataType::INT32},   // id
                           {1, DataType::VARCHAR}, // supplier
                           {2, DataType::INT32},   // rating
                           {4, DataType::VARCHAR}, // status
                           {5, DataType::INT32}    // priority
                       });

    // Final join (4–5)
    // Output columns: all from left then all from right
    plan.new_join_node(true, 4, 5, 0, 0,
                       {
                           // From left (0–1 join)
                           {0, DataType::INT32},   // id
                           {1, DataType::VARCHAR}, // region
                           {2, DataType::VARCHAR}, // country
                           {3, DataType::VARCHAR}, // category
                           {4, DataType::VARCHAR}, // subcategory

                           // From right (2–3 join)
                           {6, DataType::VARCHAR}, // supplier
                           {7, DataType::INT32},   // rating
                           {8, DataType::VARCHAR}, // status
                           {9, DataType::INT32}    // priority
                       });

    // Table type definitions
    std::vector<DataType> types1 = {DataType::INT32, DataType::VARCHAR, DataType::VARCHAR};
    std::vector<DataType> types2 = {DataType::INT32, DataType::VARCHAR, DataType::VARCHAR};
    std::vector<DataType> types3 = {DataType::INT32, DataType::VARCHAR, DataType::INT32};
    std::vector<DataType> types4 = {DataType::INT32, DataType::VARCHAR, DataType::INT32};

    // Generate data
    auto data1 = generate_random_rows(100, types1);
    auto data2 = generate_random_rows(100, types2);
    auto data3 = generate_random_rows(100, types3);
    auto data4 = generate_random_rows(100, types4);

    Table table1(std::move(data1), types1);
    Table table2(std::move(data2), types2);
    Table table3(std::move(data3), types3);
    Table table4(std::move(data4), types4);

    plan.inputs.emplace_back(table1.to_columnar());
    plan.inputs.emplace_back(table2.to_columnar());
    plan.inputs.emplace_back(table3.to_columnar());
    plan.inputs.emplace_back(table4.to_columnar());

    plan.root = 6;

    auto *context = Contest::build_context();
    auto result = Contest::execute(plan, context);
    Contest::destroy_context(context);

    auto *ref_ctx = Reference::build_context();
    auto ref_result = Reference::execute(plan, ref_ctx);
    Reference::destroy_context(ref_ctx);

    assert_tables_equal(result, ref_result);
}