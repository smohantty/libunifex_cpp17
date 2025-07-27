#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <iomanip>
#include <unifex/static_thread_pool.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/just.hpp>
#include <unifex/then.hpp>
#include <unifex/when_all.hpp>
#include <unifex/scheduler_concepts.hpp>

// Demonstration: Getting data from tasks with different return types
// and using them in a subsequent task

int main() {
    std::cout << "=== PRACTICAL EXAMPLE: USING RESULTS FROM DIFFERENT TYPES ===" << std::endl;
    std::cout << "Scenario: Task that needs int, string, and double from previous tasks" << std::endl;

    unifex::static_thread_pool pool{3};
    auto scheduler = pool.get_scheduler();

    // === APPROACH 1: SEQUENTIAL (RECOMMENDED FOR MIXED TYPES) ===
    std::cout << "\n1. SEQUENTIAL APPROACH (Clean and Simple):" << std::endl;

    auto start_time = std::chrono::steady_clock::now();

    // Get int result (e.g., user ID from database)
    auto user_id_result = unifex::sync_wait(
        unifex::schedule(scheduler) | unifex::then([]() {
            std::cout << "  [Database] Fetching user ID on thread: " << std::this_thread::get_id() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            return 12345;  // User ID (int)
        })
    );

    // Get string result (e.g., username from API)
    auto username_result = unifex::sync_wait(
        unifex::schedule(scheduler) | unifex::then([]() {
            std::cout << "  [API] Fetching username on thread: " << std::this_thread::get_id() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            return std::string("john_doe");  // Username (string)
        })
    );

    // Get double result (e.g., account balance from financial service)
    auto balance_result = unifex::sync_wait(
        unifex::schedule(scheduler) | unifex::then([]() {
            std::cout << "  [Financial] Fetching balance on thread: " << std::this_thread::get_id() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return 1234.56;  // Account balance (double)
        })
    );

    auto fetch_time = std::chrono::steady_clock::now();
    auto fetch_duration = std::chrono::duration_cast<std::chrono::milliseconds>(fetch_time - start_time);

    // === NOW USE ALL THREE RESULTS IN A SUBSEQUENT TASK ===
    if (user_id_result.has_value() && username_result.has_value() && balance_result.has_value()) {
        std::cout << "\n  ✓ All data fetched in " << fetch_duration.count() << "ms" << std::endl;
        std::cout << "  ✓ User ID: " << *user_id_result << " (type: int)" << std::endl;
        std::cout << "  ✓ Username: \"" << *username_result << "\" (type: string)" << std::endl;
        std::cout << "  ✓ Balance: $" << std::fixed << std::setprecision(2) << *balance_result << " (type: double)" << std::endl;

        // Process all three different types in a subsequent task
        auto data_processor = unifex::sync_wait(
            unifex::schedule(scheduler) | unifex::then([
                user_id = *user_id_result,      // int - clean capture
                username = *username_result,    // string - clean capture
                balance = *balance_result       // double - clean capture
            ]() {
                std::cout << "\n  [Data Processor] Combining all data on thread: " << std::this_thread::get_id() << std::endl;

                // Use all three different types naturally and safely!
                std::cout << "  ✓ Processing with clean type access:" << std::endl;
                std::cout << "    - ID: " << user_id << " (int arithmetic: " << user_id * 2 << ")" << std::endl;
                std::cout << "    - Name: \"" << username << "\" (string ops: length=" << username.length() << ")" << std::endl;
                std::cout << "    - Balance: $" << balance << " (double math: " << balance * 1.05 << " with interest)" << std::endl;

                // Perform operations combining all types
                bool is_premium = balance > 1000.0;  // Use double in condition
                std::string status = is_premium ? "Premium" : "Standard";
                std::string display_name = username + "_" + std::to_string(user_id);  // Combine string + int

                // Create final result using all three types
                std::string report = "User Report: " + display_name +
                                   " | Status: " + status +
                                   " | Balance: $" + std::to_string(balance);

                return report;
            })
        );

        if (data_processor.has_value()) {
            std::cout << "\n  ✅ FINAL COMBINED RESULT: " << *data_processor << std::endl;
            std::cout << "  ✅ Successfully used int, string, and double together!" << std::endl;
        }
    }

    auto total_time = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(total_time - start_time);
    std::cout << "  ✓ Total time: " << total_duration.count() << "ms" << std::endl;

    // === APPROACH 2: WHEN_ALL WITH SAME TYPES (WORKS WELL) ===
    std::cout << "\n2. WHEN_ALL WITH UNIFORM TYPES (Alternative):" << std::endl;

    auto uniform_start = std::chrono::steady_clock::now();

    // Convert all to strings for when_all compatibility
    auto str_user_id = unifex::schedule(scheduler) | unifex::then([]() {
        std::cout << "  [Unified] User ID as string on thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        return std::string("12345");
    });

    auto str_username = unifex::schedule(scheduler) | unifex::then([]() {
        std::cout << "  [Unified] Username on thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        return std::string("john_doe");
    });

    auto str_balance = unifex::schedule(scheduler) | unifex::then([]() {
        std::cout << "  [Unified] Balance as string on thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return std::string("1234.56");
    });

    // Use when_all with uniform string types
    auto unified_results = unifex::sync_wait(
        unifex::when_all(
            std::move(str_user_id),
            std::move(str_username),
            std::move(str_balance)
        )
    );

    auto uniform_time = std::chrono::steady_clock::now();
    auto uniform_duration = std::chrono::duration_cast<std::chrono::milliseconds>(uniform_time - uniform_start);

    if (unified_results.has_value()) {
        std::cout << "  ✓ when_all with uniform types completed in " << uniform_duration.count() << "ms" << std::endl;
        std::cout << "  ✓ Parallel execution but requires type conversion" << std::endl;

        // === ACTUAL EXTRACTION FROM unified_results ===
        std::cout << "  ✓ Now extracting REAL data from unified_results..." << std::endl;
        std::cout << "  ⚠️  unified_results type: std::optional<std::tuple<std::variant<std::tuple<std::string>>, ...>>" << std::endl;

        // Process the string results - ACTUAL EXTRACTION!
        auto string_processor = unifex::sync_wait(
            unifex::schedule(scheduler) | unifex::then([unified_results]() {
                std::cout << "  [String Processor] Processing REAL unified_results on thread: " << std::this_thread::get_id() << std::endl;

                // THIS IS THE REAL EXTRACTION - showing the actual complexity!
                std::cout << "  ✓ Extracting from complex nested types..." << std::endl;

                try {
                    // unified_results is: std::optional<std::tuple<std::variant<std::tuple<std::string>>, ...>>
                    auto& tuple_result = *unified_results;  // Get the tuple from optional

                    // Extract first result (user_id as string)
                    auto& first_variant = std::get<0>(tuple_result);   // Get first variant from tuple
                    auto& first_tuple = std::get<0>(first_variant);    // Get tuple from variant
                    std::string user_id_str = std::get<0>(first_tuple); // Get string from tuple

                    // Extract second result (username)
                    auto& second_variant = std::get<1>(tuple_result);  // Get second variant from tuple
                    auto& second_tuple = std::get<0>(second_variant);  // Get tuple from variant
                    std::string username_str = std::get<0>(second_tuple); // Get string from tuple

                    // Extract third result (balance as string)
                    auto& third_variant = std::get<2>(tuple_result);   // Get third variant from tuple
                    auto& third_tuple = std::get<0>(third_variant);    // Get tuple from variant
                    std::string balance_str = std::get<0>(third_tuple); // Get string from tuple

                    std::cout << "  ✅ SUCCESSFULLY EXTRACTED from nested types:" << std::endl;
                    std::cout << "    - std::get<0>(std::get<0>(std::get<0>(tuple))): \"" << user_id_str << "\"" << std::endl;
                    std::cout << "    - std::get<0>(std::get<0>(std::get<1>(tuple))): \"" << username_str << "\"" << std::endl;
                    std::cout << "    - std::get<0>(std::get<0>(std::get<2>(tuple))): \"" << balance_str << "\"" << std::endl;

                    // Convert back to original types if needed for processing
                    int user_id = std::stoi(user_id_str);
                    std::string username = username_str;
                    double balance = std::stod(balance_str);

                    std::cout << "  ✓ Converted to original types: ID=" << user_id << ", Name=\"" << username << "\", Balance=$" << balance << std::endl;

                    // Now we can use them together!
                    bool is_premium = balance > 1000.0;
                    std::string status = is_premium ? "Premium" : "Standard";
                    std::string display_name = username + "_" + std::to_string(user_id);

                    std::string final_report = "REAL EXTRACTION: " + display_name +
                                             " | Status: " + status +
                                             " | Balance: $" + std::to_string(balance);

                    return final_report;

                } catch (const std::exception& e) {
                    std::cout << "  ❌ ERROR during extraction: " << e.what() << std::endl;
                    return std::string("Extraction failed: ") + e.what();
                }
            })
        );

        if (string_processor.has_value()) {
            std::cout << "  ✅ " << *string_processor << std::endl;
            std::cout << "  ✅ This demonstrates the REAL complexity of when_all extraction!" << std::endl;
        }
    }

    // === COMPARISON SUMMARY ===
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "APPROACH COMPARISON FOR MIXED TYPES:" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "Sequential approach:" << std::endl;
    std::cout << "  ✅ Clean, typed access to all results" << std::endl;
    std::cout << "  ✅ Easy to pass different types to subsequent tasks" << std::endl;
    std::cout << "  ✅ Simple error handling" << std::endl;
    std::cout << "  ✅ Natural type operations (no conversion needed)" << std::endl;
    std::cout << "  ❌ No parallelism (slower overall)" << std::endl;
    std::cout << "  ⏱️  Time: " << total_duration.count() << "ms" << std::endl;
    std::cout << std::endl;
    std::cout << "when_all with uniform types:" << std::endl;
    std::cout << "  ✅ Parallel execution" << std::endl;
    std::cout << "  ❌ Requires type conversion (loss of type safety)" << std::endl;
    std::cout << "  ❌ Still complex result extraction from when_all" << std::endl;
    std::cout << "  ❌ Extra conversion overhead" << std::endl;
    std::cout << "  ⏱️  Time: " << uniform_duration.count() << "ms" << std::endl;
    std::cout << std::endl;
    std::cout << "🎯 RECOMMENDATION: Use sequential for mixed types unless" << std::endl;
    std::cout << "   parallelism is critical and worth the complexity." << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    return 0;
}