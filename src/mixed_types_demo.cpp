#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <iomanip>
#include <unifex/static_thread_pool.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/then.hpp>
#include <unifex/when_all.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <tuple>

int main() {
    std::cout << "=== UNIFEX WHEN_ALL WITH CLEAN TYPED RESULT (C++17) ===" << std::endl;

    unifex::static_thread_pool pool{3};
    auto scheduler = pool.get_scheduler();

    auto start_time = std::chrono::steady_clock::now();

    // Create typed asynchronous tasks
    auto fetch_user_id = unifex::schedule(scheduler) | unifex::then([] {
        std::cout << "  [Task] Fetching user ID on thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        return 12345;
    });

    auto fetch_username = unifex::schedule(scheduler) | unifex::then([] {
        std::cout << "  [Task] Fetching username on thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        return std::string("john_doe");
    });

    auto fetch_balance = unifex::schedule(scheduler) | unifex::then([] {
        std::cout << "  [Task] Fetching balance on thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return 1234.56;
    });

    // Combine all tasks in parallel using when_all
    auto typed_results = unifex::sync_wait(
        unifex::when_all(
            std::move(fetch_user_id),
            std::move(fetch_username),
            std::move(fetch_balance)
        )
    );

    auto fetch_end = std::chrono::steady_clock::now();
    auto fetch_duration = std::chrono::duration_cast<std::chrono::milliseconds>(fetch_end - start_time);

    if (typed_results.has_value()) {
        const auto& result_tuple = *typed_results;

        // Handle the complex nested variant/tuple structure that when_all creates
        // Result type: std::tuple<std::variant<std::tuple<int>>,
        //                        std::variant<std::tuple<std::string>>,
        //                        std::variant<std::tuple<double>>>

        // Extract user_id: std::variant<std::tuple<int>> -> std::tuple<int> -> int
        const int user_id = std::get<0>(std::get<0>(std::get<0>(result_tuple)));

        // Extract username: std::variant<std::tuple<std::string>> -> std::tuple<std::string> -> std::string
        const std::string username = std::get<0>(std::get<0>(std::get<1>(result_tuple)));

        // Extract balance: std::variant<std::tuple<double>> -> std::tuple<double> -> double
        const double balance = std::get<0>(std::get<0>(std::get<2>(result_tuple)));

        std::cout << "\n  ✅ All tasks completed in parallel (" << fetch_duration.count() << "ms)" << std::endl;
        std::cout << "    - User ID: " << user_id << " (type: int)" << std::endl;
        std::cout << "    - Username: \"" << username << "\" (type: std::string)" << std::endl;
        std::cout << "    - Balance: $" << std::fixed << std::setprecision(2) << balance << " (type: double)" << std::endl;

        // Use all three values in a follow-up task
        auto final_report = unifex::sync_wait(
            unifex::schedule(scheduler) | unifex::then([user_id, username, balance] {
                std::cout << "\n  [Processor] Processing on thread: " << std::this_thread::get_id() << std::endl;

                bool is_premium = balance > 1000.0;
                std::string status = is_premium ? "Premium" : "Standard";
                std::string display_name = username + "_" + std::to_string(user_id);

                std::string report = "User Report: " + display_name +
                                     " | Status: " + status +
                                     " | Balance: $" + std::to_string(balance);

                return report;
            })
        );

        if (final_report.has_value()) {
            std::cout << "  ✅ FINAL REPORT: " << *final_report << std::endl;
        } else {
            std::cout << "  ❌ Report generation failed" << std::endl;
        }

    } else {
        std::cout << "  ❌ Failed to fetch data" << std::endl;
    }

    auto total_end = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - start_time);
    std::cout << "\n  ⏱️  Total Time: " << total_duration.count() << "ms" << std::endl;

    std::cout << "\n📝 TYPE COMPLEXITY DEMONSTRATION:" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << "Mixed return types with when_all create complex nested types:" << std::endl;
    std::cout << "  Result type: std::tuple<" << std::endl;
    std::cout << "                 std::variant<std::tuple<int>>," << std::endl;
    std::cout << "                 std::variant<std::tuple<std::string>>," << std::endl;
    std::cout << "                 std::variant<std::tuple<double>>" << std::endl;
    std::cout << "               >" << std::endl;
    std::cout << "  Extraction requires: std::get<0>(std::get<0>(std::get<N>(result)))" << std::endl;
    std::cout << "  THREE levels of std::get are needed!" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << "💡 For simpler code, consider:" << std::endl;
    std::cout << "   • Using uniform return types (like task_dag_demo)" << std::endl;
    std::cout << "   • Converting all tasks to common type (e.g., std::string)" << std::endl;
    std::cout << "   • Using std::variant explicitly for type safety" << std::endl;

    return 0;
}
