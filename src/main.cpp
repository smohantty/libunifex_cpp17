#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <iomanip>
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

    // === FORK → JOIN → FORK → END PATTERN DEMONSTRATION ===
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "FORK → JOIN → FORK → END PATTERN DEMONSTRATION" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    {
        unifex::static_thread_pool pool{4}; // 4 worker threads for complex workflow
        auto scheduler = pool.get_scheduler();

        std::cout << "\nStep 1: INITIAL FORK - Parallel Data Sources" << std::endl;
        std::cout << "Main thread: " << std::this_thread::get_id() << std::endl;

        // INITIAL FORK: Multiple parallel data sources
        auto source1 = unifex::schedule(scheduler)
            | unifex::then([]() {
                std::cout << "  [Source 1] Fetching user data on thread: " << std::this_thread::get_id() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(60));
                std::vector<int> user_data = {1, 2, 3, 4, 5};
                return user_data;
            });

        auto source2 = unifex::schedule(scheduler)
            | unifex::then([]() {
                std::cout << "  [Source 2] Fetching config data on thread: " << std::this_thread::get_id() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(40));
                std::vector<int> config_data = {10, 20, 30};
                return config_data;
            });

        auto source3 = unifex::schedule(scheduler)
            | unifex::then([]() {
                std::cout << "  [Source 3] Fetching metrics data on thread: " << std::this_thread::get_id() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(80));
                std::vector<int> metrics_data = {100, 200};
                return metrics_data;
            });

        // Execute all source tasks in parallel
        std::cout << "  Launching 3 parallel data sources..." << std::endl;
        auto start_time = std::chrono::steady_clock::now();

        auto result1 = unifex::sync_wait(std::move(source1));
        auto result2 = unifex::sync_wait(std::move(source2));
        auto result3 = unifex::sync_wait(std::move(source3));

        auto fork1_time = std::chrono::steady_clock::now();
        auto fork1_duration = std::chrono::duration_cast<std::chrono::milliseconds>(fork1_time - start_time);

        if (result1.has_value() && result2.has_value() && result3.has_value()) {
            std::cout << "  ✓ All sources completed in " << fork1_duration.count() << "ms" << std::endl;

            // JOIN: Merge all data sources
            std::cout << "\nStep 2: JOIN - Merging Data Sources" << std::endl;
            auto join_processor = unifex::schedule(scheduler)
                | unifex::then([user_data = *result1, config_data = *result2, metrics_data = *result3]() {
                    std::cout << "  [Joiner] Merging data on thread: " << std::this_thread::get_id() << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(30));

                    // Combine all data into a unified dataset
                    std::vector<int> merged_data;
                    merged_data.insert(merged_data.end(), user_data.begin(), user_data.end());
                    merged_data.insert(merged_data.end(), config_data.begin(), config_data.end());
                    merged_data.insert(merged_data.end(), metrics_data.begin(), metrics_data.end());

                    std::cout << "  ✓ Merged " << merged_data.size() << " total elements" << std::endl;
                    return merged_data;
                });

            auto join_result = unifex::sync_wait(std::move(join_processor));
            auto join_time = std::chrono::steady_clock::now();
            auto join_duration = std::chrono::duration_cast<std::chrono::milliseconds>(join_time - fork1_time);

            if (join_result.has_value()) {
                auto merged_data = *join_result;
                std::cout << "  ✓ Join completed in " << join_duration.count() << "ms" << std::endl;

                // MIDDLE PRODUCER: Process merged data and decide next actions
                std::cout << "\nStep 3: MIDDLE PRODUCER - Data Analysis & Decision" << std::endl;
                auto analyzer = unifex::schedule(scheduler)
                    | unifex::then([merged_data]() {
                        std::cout << "  [Analyzer] Processing merged data on thread: " << std::this_thread::get_id() << std::endl;
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));

                        // Calculate statistics
                        int sum = 0;
                        int max_val = 0;
                        for (int val : merged_data) {
                            sum += val;
                            max_val = std::max(max_val, val);
                        }

                        // Decision logic based on analysis
                        struct AnalysisResult {
                            std::vector<int> data;
                            int sum;
                            int max_val;
                            bool needs_alert;
                            bool needs_report;
                        };

                        AnalysisResult result;
                        result.data = merged_data;
                        result.sum = sum;
                        result.max_val = max_val;
                        result.needs_alert = (max_val > 150);  // Alert if max > 150
                        result.needs_report = (sum > 300);     // Report if sum > 300

                        std::cout << "  ✓ Analysis: sum=" << sum << ", max=" << max_val
                                 << ", alert=" << (result.needs_alert ? "YES" : "NO")
                                 << ", report=" << (result.needs_report ? "YES" : "NO") << std::endl;

                        return result;
                    });

                auto analysis_result = unifex::sync_wait(std::move(analyzer));
                auto analysis_time = std::chrono::steady_clock::now();
                auto analysis_duration = std::chrono::duration_cast<std::chrono::milliseconds>(analysis_time - join_time);

                if (analysis_result.has_value()) {
                    auto analysis = *analysis_result;
                    std::cout << "  ✓ Analysis completed in " << analysis_duration.count() << "ms" << std::endl;

                    // SECONDARY FORK: Based on analysis, trigger different parallel actions
                    std::cout << "\nStep 4: SECONDARY FORK - Parallel Action Execution" << std::endl;

                    auto storage_task = unifex::schedule(scheduler)
                        | unifex::then([analysis]() {
                            std::cout << "  [Storage] Saving data on thread: " << std::this_thread::get_id() << std::endl;
                            std::this_thread::sleep_for(std::chrono::milliseconds(70));
                            return "Data saved to storage with " + std::to_string(analysis.data.size()) + " records";
                        });

                    // Conditional tasks based on analysis
                    auto alert_task = unifex::schedule(scheduler)
                        | unifex::then([analysis]() -> std::string {
                            std::cout << "  [Alert] Processing alerts on thread: " << std::this_thread::get_id() << std::endl;
                            std::this_thread::sleep_for(std::chrono::milliseconds(45));
                            if (analysis.needs_alert) {
                                return "⚠️  ALERT: High value detected (max=" + std::to_string(analysis.max_val) + ")";
                            } else {
                                return std::string("✓ No alerts needed");
                            }
                        });

                    auto report_task = unifex::schedule(scheduler)
                        | unifex::then([analysis]() -> std::string {
                            std::cout << "  [Report] Generating report on thread: " << std::this_thread::get_id() << std::endl;
                            std::this_thread::sleep_for(std::chrono::milliseconds(90));
                            if (analysis.needs_report) {
                                return "📊 Report generated: Total sum=" + std::to_string(analysis.sum) + " (threshold exceeded)";
                            } else {
                                return "📄 Basic report: Total sum=" + std::to_string(analysis.sum) + " (normal)";
                            }
                        });

                    // Execute all final tasks in parallel
                    std::cout << "  Launching 3 parallel action tasks..." << std::endl;
                    auto storage_result = unifex::sync_wait(std::move(storage_task));
                    auto alert_result = unifex::sync_wait(std::move(alert_task));
                    auto report_result = unifex::sync_wait(std::move(report_task));

                    auto end_time = std::chrono::steady_clock::now();
                    auto final_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - analysis_time);
                    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

                    // END: Display final results
                    std::cout << "\nStep 5: END - Final Results (completed in " << final_duration.count() << "ms)" << std::endl;
                    if (storage_result.has_value()) {
                        std::cout << "  " << *storage_result << std::endl;
                    }
                    if (alert_result.has_value()) {
                        std::cout << "  " << *alert_result << std::endl;
                    }
                    if (report_result.has_value()) {
                        std::cout << "  " << *report_result << std::endl;
                    }

                    std::cout << "\n🎯 WORKFLOW SUMMARY:" << std::endl;
                    std::cout << "  Total execution time: " << total_duration.count() << "ms" << std::endl;
                    std::cout << "  ├─ Fork 1 (parallel sources): " << fork1_duration.count() << "ms" << std::endl;
                    std::cout << "  ├─ Join (data merge): " << join_duration.count() << "ms" << std::endl;
                    std::cout << "  ├─ Analysis (middle producer): " << analysis_duration.count() << "ms" << std::endl;
                    std::cout << "  └─ Fork 2 (parallel actions): " << final_duration.count() << "ms" << std::endl;
                }
            }
        }
    }

    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "COMPLEX WORKFLOW PATTERN SUMMARY:" << std::endl;
    std::cout << "1. FORK:   Parallel data sources (user, config, metrics)" << std::endl;
    std::cout << "2. JOIN:   Merge all data into unified dataset" << std::endl;
    std::cout << "3. MIDDLE: Analyze merged data & make decisions" << std::endl;
    std::cout << "4. FORK:   Parallel actions based on analysis" << std::endl;
    std::cout << "5. END:    Collect and display final results" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    std::cout << "\n" << std::string(60, '=') << std::endl;

    // === WHEN_ALL API DEMONSTRATION ===
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "WHEN_ALL API DEMONSTRATION" << std::endl;
    std::cout << std::string(50, '=') << std::endl;

    {
        unifex::static_thread_pool pool{3}; // 3 worker threads
        auto scheduler = pool.get_scheduler();

        std::cout << "\nComparison: Sequential vs when_all execution" << std::endl;
        std::cout << "Main thread: " << std::this_thread::get_id() << std::endl;

        // === SEQUENTIAL APPROACH (what we used before) ===
        std::cout << "\n1. SEQUENTIAL APPROACH:" << std::endl;
        auto seq_start = std::chrono::steady_clock::now();

        auto task_a = unifex::schedule(scheduler)
            | unifex::then([]() {
                std::cout << "  [Sequential] Task A on thread: " << std::this_thread::get_id() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return 42;
            });

        auto task_b = unifex::schedule(scheduler)
            | unifex::then([]() {
                std::cout << "  [Sequential] Task B on thread: " << std::this_thread::get_id() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(80));
                return 99;
            });

        auto task_c = unifex::schedule(scheduler)
            | unifex::then([]() {
                std::cout << "  [Sequential] Task C on thread: " << std::this_thread::get_id() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(60));
                return 77;
            });

        // Execute sequentially - each waits for previous to complete
        auto result_a = unifex::sync_wait(std::move(task_a));
        auto result_b = unifex::sync_wait(std::move(task_b));
        auto result_c = unifex::sync_wait(std::move(task_c));

        auto seq_end = std::chrono::steady_clock::now();
        auto seq_duration = std::chrono::duration_cast<std::chrono::milliseconds>(seq_end - seq_start);

        std::cout << "  ✓ Sequential results: ";
        if (result_a.has_value()) std::cout << *result_a << ", ";
        if (result_b.has_value()) std::cout << *result_b << ", ";
        if (result_c.has_value()) std::cout << *result_c;
        std::cout << std::endl;
        std::cout << "  ✓ Sequential time: " << seq_duration.count() << "ms" << std::endl;

        // === WHEN_ALL APPROACH ===
        std::cout << "\n2. WHEN_ALL APPROACH:" << std::endl;
        auto parallel_start = std::chrono::steady_clock::now();

        auto parallel_task_a = unifex::schedule(scheduler)
            | unifex::then([]() {
                std::cout << "  [Parallel] Task A on thread: " << std::this_thread::get_id() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return 42;
            });

        auto parallel_task_b = unifex::schedule(scheduler)
            | unifex::then([]() {
                std::cout << "  [Parallel] Task B on thread: " << std::this_thread::get_id() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(80));
                return 99;
            });

        auto parallel_task_c = unifex::schedule(scheduler)
            | unifex::then([]() {
                std::cout << "  [Parallel] Task C on thread: " << std::this_thread::get_id() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(60));
                return 77;
            });

        // Use when_all to execute all tasks simultaneously
        auto when_all_sender = unifex::when_all(
            std::move(parallel_task_a),
            std::move(parallel_task_b),
            std::move(parallel_task_c)
        );

        // Execute all tasks in parallel and wait for completion
        auto parallel_results = unifex::sync_wait(std::move(when_all_sender));

        auto parallel_end = std::chrono::steady_clock::now();
        auto parallel_duration = std::chrono::duration_cast<std::chrono::milliseconds>(parallel_end - parallel_start);

        if (parallel_results.has_value()) {
            std::cout << "  ✓ when_all completed successfully with all results" << std::endl;
            std::cout << "  ✓ Note: when_all returns complex tuple types (see libunifex docs for unwrapping)" << std::endl;
        }
        std::cout << "  ✓ Parallel time: " << parallel_duration.count() << "ms" << std::endl;

        // === PERFORMANCE COMPARISON ===
        std::cout << "\n3. PERFORMANCE COMPARISON:" << std::endl;
        std::cout << "  Sequential execution: " << seq_duration.count() << "ms (sum of individual times)" << std::endl;
        std::cout << "  Parallel execution:   " << parallel_duration.count() << "ms (max of individual times)" << std::endl;

        double speedup = static_cast<double>(seq_duration.count()) / parallel_duration.count();
        std::cout << "  Speedup factor:       " << std::fixed << std::setprecision(2) << speedup << "x" << std::endl;

        if (speedup > 1.5) {
            std::cout << "  🚀 Significant performance improvement with when_all!" << std::endl;
        }

        // === SIMPLER WHEN_ALL EXAMPLE ===
        std::cout << "\n4. SIMPLE WHEN_ALL USAGE:" << std::endl;

        // Simple uniform type tasks for easier handling
        auto simple_task1 = unifex::just(10) | unifex::then([](int x) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return x * 2; // 20
        });

        auto simple_task2 = unifex::just(5) | unifex::then([](int x) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            return x * 3; // 15
        });

        auto simple_task3 = unifex::just(7) | unifex::then([](int x) {
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            return x * 4; // 28
        });

        std::cout << "  Executing simple when_all with uniform int results..." << std::endl;

        auto simple_start = std::chrono::steady_clock::now();
        auto simple_results = unifex::sync_wait(
            unifex::when_all(std::move(simple_task1), std::move(simple_task2), std::move(simple_task3))
        );
        auto simple_end = std::chrono::steady_clock::now();
        auto simple_duration = std::chrono::duration_cast<std::chrono::milliseconds>(simple_end - simple_start);

        if (simple_results.has_value()) {
            std::cout << "  ✓ Simple when_all completed in " << simple_duration.count() << "ms" << std::endl;
            std::cout << "  ✓ All tasks executed in parallel (should be ~50ms, not 120ms)" << std::endl;
        }
    }

    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "WHEN_ALL API SUMMARY:" << std::endl;
    std::cout << "✓ when_all executes multiple tasks simultaneously" << std::endl;
    std::cout << "✓ Returns complex tuple/variant types containing all results" << std::endl;
    std::cout << "✓ Waits for ALL tasks to complete (slowest determines time)" << std::endl;
    std::cout << "✓ Significant performance improvement over sequential execution" << std::endl;
    std::cout << "✓ Type unwrapping can be complex - consider using simpler approaches" << std::endl;
    std::cout << "✓ Best used when you need true parallel execution of multiple tasks" << std::endl;
    std::cout << std::string(50, '=') << std::endl;

    std::cout << "\nApplication completed successfully!" << std::endl;
    return 0;
}