#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <iomanip>
#include <tuple>
#include <stdexcept>
#include <random>
#include <unifex/static_thread_pool.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/just.hpp>
#include <unifex/then.hpp>
#include <unifex/when_all.hpp>
#include <unifex/scheduler_concepts.hpp>

/*
 * TASK DEPENDENCY GRAPH (DAG) WITH ERROR HANDLING:
 *
 *     Task1 ────┐
 *              ├───► Task4 ────┐
 *     Task2 ────┤              │
 *              │               ├───► Task6
 *              └───► Task5 ────┘
 *              ┌───►        ▲
 *     Task3 ────┘            │
 *
 * Execution Flow:
 * 1. Task1, Task2, Task3 run independently in parallel
 * 2. Task4 depends on (Task1 + Task2) results
 * 3. Task5 depends on (Task1 + Task2 + Task3) results
 * 4. Task6 depends on (Task4 + Task5) results
 *
 * Error Handling Features:
 * - Each task can potentially fail with different error types
 * - Pipeline stops immediately when any task fails
 * - Detailed error reporting shows failure location and cause
 * - Graceful cleanup and resource management
 * - Configurable failure simulation for testing
 */

// Custom exception types for different failure scenarios
class TaskExecutionError : public std::runtime_error {
public:
    TaskExecutionError(const std::string& task_name, const std::string& reason)
        : std::runtime_error("Task " + task_name + " failed: " + reason)
        , task_name_(task_name), reason_(reason) {}

    const std::string& get_task_name() const { return task_name_; }
    const std::string& get_reason() const { return reason_; }

private:
    std::string task_name_;
    std::string reason_;
};

class DataValidationError : public TaskExecutionError {
public:
    DataValidationError(const std::string& task_name, const std::string& invalid_data)
        : TaskExecutionError(task_name, "Invalid data: " + invalid_data) {}
};

class ResourceExhaustionError : public TaskExecutionError {
public:
    ResourceExhaustionError(const std::string& task_name, const std::string& resource)
        : TaskExecutionError(task_name, "Resource exhausted: " + resource) {}
};

// Configuration for simulating failures (set to true to simulate specific failures)
struct FailureConfig {
    bool task1_fail = false;
    bool task2_fail = false;
    bool task3_fail = false;
    bool task4_fail = false;
    bool task5_fail = false;
    bool task6_fail = false;

    // For random testing
    bool enable_random_failures = false;
    double failure_probability = 0.2;  // 20% chance per task
};

template<typename... Ts>
auto unwrap_when_all(const std::tuple<Ts...>& when_all_result) {
    return std::apply([](const auto&... variants) {
        return std::make_tuple(std::get<0>(std::get<0>(variants))...);
    }, when_all_result);
}

void print_error_summary(const std::string& level, const std::string& task_name,
                        const std::exception& e, const std::chrono::steady_clock::time_point& start_time) {
    auto error_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(error_time - start_time);

    std::cout << "\n💥 ERROR OCCURRED IN " << level << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << "❌ Failed Task: " << task_name << std::endl;
    std::cout << "🕐 Time of Failure: " << elapsed.count() << "ms after start" << std::endl;
    std::cout << "📋 Error Details: " << e.what() << std::endl;
    std::cout << "🚫 Pipeline Status: TERMINATED - All subsequent tasks cancelled" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
}

int main() {
    std::cout << "=== UNIFEX TASK DAG WITH ERROR HANDLING DEMO ===" << std::endl;
    std::cout << "Demonstrating robust error handling in async task pipelines\n" << std::endl;

    // Configure failure scenarios (modify these to test different failure points)
    FailureConfig config;
    // Uncomment one of these lines to test specific failures:
    // config.task2_fail = true;  // Test Level 1 failure
    // config.task5_fail = true;  // Test Level 2 failure
    // config.task6_fail = true;  // Test Level 3 failure
    config.enable_random_failures = false;  // Enable random failures for realistic testing

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    unifex::static_thread_pool pool{4};
    auto scheduler = pool.get_scheduler();

    auto total_start = std::chrono::steady_clock::now();

    try {
        // ===== LEVEL 1: Independent Tasks (Parallel Execution) =====
        std::cout << "🚀 Starting Level 1: Independent tasks (Task1, Task2, Task3)" << std::endl;

        auto task1 = unifex::schedule(scheduler) | unifex::then([&config, &gen, &dis] {
            std::cout << "  [Task1] Processing data source A on thread: " << std::this_thread::get_id() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // Simulate potential failure
            if (config.task1_fail || (config.enable_random_failures && dis(gen) < config.failure_probability)) {
                throw DataValidationError("Task1", "DataSourceA contains corrupted records");
            }

            return 42.5;
        });

        auto task2 = unifex::schedule(scheduler) | unifex::then([&config, &gen, &dis] {
            std::cout << "  [Task2] Processing data source B on thread: " << std::this_thread::get_id() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(80));

            // Simulate potential failure
            if (config.task2_fail || (config.enable_random_failures && dis(gen) < config.failure_probability)) {
                throw ResourceExhaustionError("Task2", "Database connection pool exhausted");
            }

            return 73.2;
        });

        auto task3 = unifex::schedule(scheduler) | unifex::then([&config, &gen, &dis] {
            std::cout << "  [Task3] Processing data source C on thread: " << std::this_thread::get_id() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(120));

            // Simulate potential failure
            if (config.task3_fail || (config.enable_random_failures && dis(gen) < config.failure_probability)) {
                throw TaskExecutionError("Task3", "Network timeout while fetching external API data");
            }

            return 91.8;
        });

        // Wait for all Level 1 tasks to complete with error handling
        auto level1_results = unifex::sync_wait(
            unifex::when_all(
                std::move(task1),
                std::move(task2),
                std::move(task3)
            )
        );

        if (!level1_results.has_value()) {
            std::cout << "❌ Level 1 tasks were cancelled or completed with done signal!" << std::endl;
            return 1;
        }

        // Extract results - when_all creates std::variant<std::tuple<T>> nesting
        auto [result1, result2, result3] = unwrap_when_all(*level1_results);

        std::cout << "✅ Level 1 completed successfully:" << std::endl;
        std::cout << "    Task1: DataSourceA = " << std::fixed << std::setprecision(1) << result1 << std::endl;
        std::cout << "    Task2: DataSourceB = " << result2 << std::endl;
        std::cout << "    Task3: DataSourceC = " << result3 << std::endl;

        // Store values for C++17 lambda capture compatibility
        double r1 = result1, r2 = result2, r3 = result3;

        // ===== LEVEL 2: Dependent Tasks =====
        std::cout << "\n🔄 Starting Level 2: Dependent tasks (Task4, Task5)" << std::endl;

        auto task4 = unifex::schedule(scheduler) | unifex::then([r1, r2, &config, &gen, &dis] {
            std::cout << "  [Task4] Combining DataSourceA + DataSourceB on thread: " << std::this_thread::get_id() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(60));

            // Simulate potential failure
            if (config.task4_fail || (config.enable_random_failures && dis(gen) < config.failure_probability)) {
                throw DataValidationError("Task4", "Result validation failed - combined value out of expected range");
            }

            return r1 + r2;
        });

        auto task5 = unifex::schedule(scheduler) | unifex::then([r1, r2, r3, &config, &gen, &dis] {
            std::cout << "  [Task5] Aggregating all data sources on thread: " << std::this_thread::get_id() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(90));

            // Simulate potential failure
            if (config.task5_fail || (config.enable_random_failures && dis(gen) < config.failure_probability)) {
                throw ResourceExhaustionError("Task5", "Insufficient memory for aggregation operation");
            }

            return (r1 + r2 + r3) / 3.0;
        });

        // Wait for Level 2 tasks to complete with error handling
        auto level2_results = unifex::sync_wait(
            unifex::when_all(
                std::move(task4),
                std::move(task5)
            )
        );

        if (!level2_results.has_value()) {
            std::cout << "❌ Level 2 tasks were cancelled or completed with done signal!" << std::endl;
            return 1;
        }

        auto [result4, result5] = unwrap_when_all(*level2_results);

        std::cout << "✅ Level 2 completed successfully:" << std::endl;
        std::cout << "    Task4: CombinedAB = " << std::fixed << std::setprecision(2) << result4 << std::endl;
        std::cout << "    Task5: AggregatedABC = " << result5 << std::endl;

        // Store values for C++17 lambda capture compatibility
        double r4 = result4, r5 = result5;

        // ===== LEVEL 3: Final Task =====
        std::cout << "\n🎯 Starting Level 3: Final task (Task6)" << std::endl;

        auto task6_result = unifex::sync_wait(
            unifex::schedule(scheduler) | unifex::then([r4, r5, &config, &gen, &dis] {
                std::cout << "  [Task6] Final processing on thread: " << std::this_thread::get_id() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));

                // Simulate potential failure
                if (config.task6_fail || (config.enable_random_failures && dis(gen) < config.failure_probability)) {
                    throw TaskExecutionError("Task6", "Final validation failed - output format incompatible");
                }

                return (r4 * 0.6) + (r5 * 0.4);
            })
        );

        if (!task6_result.has_value()) {
            std::cout << "❌ Level 3 task was cancelled or completed with done signal!" << std::endl;
            return 1;
        }

        const double result6 = *task6_result;
        std::cout << "✅ Level 3 completed successfully:" << std::endl;
        std::cout << "    Task6: FinalScore = " << std::fixed << std::setprecision(2) << result6 << std::endl;

        // ===== SUCCESS SUMMARY =====
        auto total_end = std::chrono::steady_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start);

        std::cout << "\n🎉 PIPELINE COMPLETED SUCCESSFULLY!" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        std::cout << "📊 Final Results:" << std::endl;
        std::cout << "  Level 1: Task1=" << result1 << ", Task2=" << result2 << ", Task3=" << result3 << std::endl;
        std::cout << "  Level 2: Task4=" << std::fixed << std::setprecision(2) << result4 << ", Task5=" << result5 << std::endl;
        std::cout << "  Level 3: Task6=" << result6 << " (final weighted score)" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        std::cout << "🕐 Total Execution Time: " << total_duration.count() << "ms" << std::endl;
        std::cout << "🎯 Final Result: " << std::fixed << std::setprecision(2) << result6 << std::endl;

    } catch (const DataValidationError& e) {
        print_error_summary("DATA VALIDATION", e.get_task_name(), e, total_start);
        return 1;
    } catch (const ResourceExhaustionError& e) {
        print_error_summary("RESOURCE MANAGEMENT", e.get_task_name(), e, total_start);
        return 1;
    } catch (const TaskExecutionError& e) {
        print_error_summary("TASK EXECUTION", e.get_task_name(), e, total_start);
        return 1;
    } catch (const std::exception& e) {
        print_error_summary("UNKNOWN ERROR", "Unknown", e, total_start);
        return 1;
    }

    std::cout << "\n💡 Error Handling Features Demonstrated:" << std::endl;
    std::cout << "  • Custom exception types for different failure categories" << std::endl;
    std::cout << "  • Automatic pipeline termination on first failure" << std::endl;
    std::cout << "  • Detailed error reporting with failure location and timing" << std::endl;
    std::cout << "  • Graceful cleanup and resource management" << std::endl;
    std::cout << "  • Configurable failure simulation for testing robustness" << std::endl;
    std::cout << "  • when_all stops all tasks when any task fails" << std::endl;

    return 0;
}
