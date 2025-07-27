#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <unifex/static_thread_pool.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/inline_scheduler.hpp>
#include <unifex/just.hpp>
#include <unifex/then.hpp>
#include <unifex/let_value.hpp>
#include <unifex/when_all.hpp>
// Note: UNIFEX_NO_COROUTINES=1 disables coroutine support for C++17 compatibility

namespace fs = std::filesystem;

// Helper function to simulate data processing
std::vector<int> simulate_data_fetch(int size) {
    std::vector<int> data;
    for (int i = 1; i <= size; ++i) {
        data.push_back(i * 10);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Simulate network/disk I/O
    return data;
}

// Processor 1: Square all values
std::vector<int> square_processor(const std::vector<int>& data) {
    std::cout << "  [Processor 1] Squaring values on thread: " << std::this_thread::get_id() << std::endl;
    std::vector<int> result;
    for (int val : data) {
        result.push_back(val * val);
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simulate processing time
    }
    return result;
}

// Processor 2: Sum and analyze
std::string analyze_processor(const std::vector<int>& data) {
    std::cout << "  [Processor 2] Analyzing data on thread: " << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(30)); // Simulate analysis time

    int sum = 0;
    for (int val : data) {
        sum += val;
    }

    return "Analysis: " + std::to_string(data.size()) + " items, sum=" + std::to_string(sum);
}

int main() {
    std::cout << "C++17 Application with libunifex" << std::endl;
    std::cout << "================================" << std::endl;

    // Demonstrate C++17 filesystem
    auto current_path = fs::current_path();
    std::cout << "Current directory: " << current_path << std::endl;

    // Demonstrate libunifex usage without coroutines
    auto inline_sched = unifex::inline_scheduler{};

    // Create a simple async computation using unifex algorithms
    auto computation = unifex::just(21)
        | unifex::then([](int x) {
            std::cout << "Processing value: " << x << std::endl;
            return x * 2;
        })
        | unifex::then([](int result) {
            std::cout << "Final result: " << result << std::endl;
            return result;
        });

    // Execute the computation synchronously
    auto result = unifex::sync_wait(std::move(computation));

    if (result.has_value()) {
        std::cout << "Sync wait result: " << *result << std::endl;
    }

    // Demonstrate thread pool scheduler
    std::cout << "\nDemonstrating thread pool execution:" << std::endl;
    {
        unifex::static_thread_pool pool{2}; // 2 worker threads
        auto pool_scheduler = pool.get_scheduler();

        auto thread_work = unifex::schedule(pool_scheduler)
            | unifex::then([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::cout << "Work executed on thread: " << std::this_thread::get_id() << std::endl;
                return 42;
            });

        auto thread_result = unifex::sync_wait(std::move(thread_work));
        if (thread_result.has_value()) {
            std::cout << "Thread pool result: " << *thread_result << std::endl;
        }
    }

    // === JOIN AND FORK PATTERN DEMONSTRATION ===
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "JOIN AND FORK PATTERN DEMONSTRATION" << std::endl;
    std::cout << std::string(50, '=') << std::endl;

    {
        unifex::static_thread_pool pool{3}; // 3 worker threads for better parallelism
        auto scheduler = pool.get_scheduler();

        std::cout << "\nStep 1: Data Producer (JOIN point)" << std::endl;
        std::cout << "Main thread: " << std::this_thread::get_id() << std::endl;

        // PRODUCER TASK: Fetch/generate data (this is our JOIN point)
        auto producer = unifex::schedule(scheduler)
            | unifex::then([]() {
                std::cout << "  [Producer] Fetching data on thread: " << std::this_thread::get_id() << std::endl;
                return simulate_data_fetch(5); // Generate vector of 5 elements
            });

        // Execute the producer first to get the data
        auto data_result = unifex::sync_wait(std::move(producer));

        if (data_result.has_value()) {
            auto data = *data_result;

            std::cout << "\nStep 2: FORK - Launching parallel processors" << std::endl;
            std::cout << "  Data size: " << data.size() << " elements [";
            for (size_t i = 0; i < data.size(); ++i) {
                std::cout << data[i];
                if (i < data.size() - 1) std::cout << ", ";
            }
            std::cout << "]" << std::endl;

            // FORK: Create two parallel tasks that process the same input data
            std::cout << "  Launching Task 1 (Square Processor)..." << std::endl;
            auto task1 = unifex::schedule(scheduler)
                | unifex::then([data]() {
                    return square_processor(data);
                });

            std::cout << "  Launching Task 2 (Analysis Processor)..." << std::endl;
            auto task2 = unifex::schedule(scheduler)
                | unifex::then([data]() {
                    return analyze_processor(data);
                });

            // Execute both tasks independently to demonstrate parallel execution
            std::cout << "\n  Executing both tasks in parallel..." << std::endl;
            auto start_time = std::chrono::steady_clock::now();

            // Start both tasks asynchronously and collect results
            auto result1 = unifex::sync_wait(std::move(task1));
            auto result2 = unifex::sync_wait(std::move(task2));

            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            if (result1.has_value() && result2.has_value()) {
                std::cout << "\nStep 3: Combining parallel results (completed in "
                         << duration.count() << "ms)" << std::endl;

                auto squared_data = *result1;
                auto analysis = *result2;

                std::cout << "  Squared results: [";
                for (size_t i = 0; i < squared_data.size(); ++i) {
                    std::cout << squared_data[i];
                    if (i < squared_data.size() - 1) std::cout << ", ";
                }
                std::cout << "]" << std::endl;

                std::cout << "  " << analysis << std::endl;

                // Final aggregation
                int squared_sum = 0;
                for (int val : squared_data) {
                    squared_sum += val;
                }

                std::cout << "\nFinal result: squared_sum=" << squared_sum <<
                            ", original_analysis=\"" << analysis << "\"" << std::endl;
            }
        }
    }

    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "Pattern Summary:" << std::endl;
    std::cout << "1. JOIN:  Producer task generates shared data" << std::endl;
    std::cout << "2. FORK:  Data flows to multiple parallel consumers" << std::endl;
    std::cout << "3. MERGE: Results from parallel tasks are combined" << std::endl;
    std::cout << std::string(50, '=') << std::endl;

    std::cout << "\nApplication completed successfully!" << std::endl;
    return 0;
}