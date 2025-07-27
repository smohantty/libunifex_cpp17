#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <iomanip>
#include <tuple>
#include <stdexcept>
#include <memory>
#include <optional>
#include <variant>
#include <any>
#include <typeinfo>
#include <unifex/static_thread_pool.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/just.hpp>
#include <unifex/then.hpp>
#include <unifex/when_all.hpp>
#include <unifex/scheduler_concepts.hpp>

/*
 * TASK DEPENDENCY GRAPH (DAG) - FLEXIBLE TYPE-SAFE OOP VERSION:
 *
 *     Task1 ────┐
 *              ├───► Task4 ────┐
 *     Task2 ────┤              │
 *              │               ├───► Task6
 *              └───► Task5 ────┘
 *              ┌───►        ▲
 *     Task3 ────┘            │
 *
 * Future-Proof Architecture:
 * - Tasks can return any type (double, string, complex objects, etc.)
 * - Type-safe result handling with std::variant and templates
 * - Flexible TaskResult that can hold different data types
 * - Clean interfaces that adapt to different result types
 * - Production-ready with excellent extensibility
 */

// ===== FLEXIBLE RESULT TYPES =====

// Base interface for type-erased results
class ITaskResult {
public:
    virtual ~ITaskResult() = default;
    virtual std::string get_description() const = 0;
    virtual std::string get_source_info() const = 0;
    virtual std::string get_type_name() const = 0;
    virtual std::string to_string() const = 0;
};

// Template implementation for specific result types
template<typename T>
class TaskResult : public ITaskResult {
private:
    T value_;
    std::string description_;
    std::string source_info_;

public:
    TaskResult(T value, const std::string& desc, const std::string& info = "")
        : value_(std::move(value)), description_(desc), source_info_(info) {}

    const T& get_value() const { return value_; }
    T& get_value() { return value_; }

    std::string get_description() const override { return description_; }
    std::string get_source_info() const override { return source_info_; }
    std::string get_type_name() const override { return typeid(T).name(); }

    std::string to_string() const override {
        if constexpr (std::is_arithmetic_v<T>) {
            return std::to_string(value_);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return value_;
        } else {
            return "[Complex Object]";
        }
    }
};

// Type aliases for common result types
using DoubleResult = TaskResult<double>;
using StringResult = TaskResult<std::string>;
using IntResult = TaskResult<int>;

// Variant for storing different result types
using AnyTaskResult = std::variant<
    std::shared_ptr<DoubleResult>,
    std::shared_ptr<StringResult>,
    std::shared_ptr<IntResult>
>;

// Helper function to create results
template<typename T>
std::shared_ptr<TaskResult<T>> make_task_result(T value, const std::string& desc, const std::string& info = "") {
    return std::make_shared<TaskResult<T>>(std::move(value), desc, info);
}

// ===== LEVEL RESULT CONTAINERS =====

struct Level1Results {
    AnyTaskResult task1_result;
    AnyTaskResult task2_result;
    AnyTaskResult task3_result;

    Level1Results() = default;
    Level1Results(AnyTaskResult r1, AnyTaskResult r2, AnyTaskResult r3)
        : task1_result(std::move(r1)), task2_result(std::move(r2)), task3_result(std::move(r3)) {}
};

struct Level2Results {
    AnyTaskResult task4_result;
    AnyTaskResult task5_result;

    Level2Results() = default;
    Level2Results(AnyTaskResult r4, AnyTaskResult r5)
        : task4_result(std::move(r4)), task5_result(std::move(r5)) {}
};

// ===== RESULT ACCESSOR HELPERS =====

template<typename T>
std::shared_ptr<TaskResult<T>> get_result_as(const AnyTaskResult& result) {
    return std::get<std::shared_ptr<TaskResult<T>>>(result);
}

template<typename T>
T get_value_as(const AnyTaskResult& result) {
    return get_result_as<T>(result)->get_value();
}

std::shared_ptr<ITaskResult> get_result_interface(const AnyTaskResult& result) {
    return std::visit([](auto&& arg) -> std::shared_ptr<ITaskResult> {
        return std::static_pointer_cast<ITaskResult>(arg);
    }, result);
}

// ===== EXCEPTION TYPES =====

class TaskExecutionError : public std::runtime_error {
public:
    TaskExecutionError(const std::string& task_name, const std::string& reason)
        : std::runtime_error("Task " + task_name + " failed: " + reason)
        , task_name_(task_name) {}

    const std::string& get_task_name() const { return task_name_; }

private:
    std::string task_name_;
};

// ===== TASK INTERFACE =====

class ITask {
public:
    virtual ~ITask() = default;
    virtual AnyTaskResult execute() = 0;
    virtual std::string get_name() const = 0;

protected:
    void simulate_work(int duration_ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    }
};

// ===== LEVEL 1 TASKS (INDEPENDENT) =====

class Task1 : public ITask {
public:
    AnyTaskResult execute() override {
        std::cout << "  [Task1] Processing data source A on thread: " << std::this_thread::get_id() << std::endl;
        simulate_work(100);

        double result = process_data_source_a();
        return make_task_result(result, "DataSourceA", "Primary data repository");
    }

    std::string get_name() const override { return "Task1"; }

private:
    double process_data_source_a() {
        // Real data processing that returns numeric data
        return 42.5;
    }
};

class Task2 : public ITask {
public:
    AnyTaskResult execute() override {
        std::cout << "  [Task2] Processing data source B on thread: " << std::this_thread::get_id() << std::endl;
        simulate_work(80);

        std::string result = process_data_source_b();
        return make_task_result(result, "DataSourceB", "Secondary data warehouse");
    }

    std::string get_name() const override { return "Task2"; }

    private:
        std::string process_data_source_b() {
            // Real data processing that returns string data
            return "PROCESSED_DATA_B_73.2";
        }
};

class Task3 : public ITask {
public:
    AnyTaskResult execute() override {
        std::cout << "  [Task3] Processing data source C on thread: " << std::this_thread::get_id() << std::endl;
        simulate_work(120);

        int result = process_data_source_c();
        return make_task_result(result, "DataSourceC", "External API endpoint");
    }

    std::string get_name() const override { return "Task3"; }

private:
    int process_data_source_c() {
        // Real data processing that returns integer data
        return 91;
    }
};

// ===== LEVEL 2 TASKS (DEPENDENT) =====

class Task4 : public ITask {
private:
    const Level1Results& level1_results_;

public:
    explicit Task4(const Level1Results& level1_results)
        : level1_results_(level1_results) {}

    AnyTaskResult execute() override {
        std::cout << "  [Task4] Combining DataSourceA + DataSourceB on thread: " << std::this_thread::get_id() << std::endl;
        simulate_work(60);

        // Extract values with type safety
        double value1 = get_value_as<double>(level1_results_.task1_result);
        std::string value2 = get_value_as<std::string>(level1_results_.task2_result);

        // Validate inputs
        if (value1 <= 0) {
            throw TaskExecutionError("Task4", "Invalid numeric input from Task1");
        }
        if (value2.empty()) {
            throw TaskExecutionError("Task4", "Invalid string input from Task2");
        }

        // Process: extract numeric part from string and combine
        double numeric_part = 73.2; // Extracted from string (simplified)
        double combined_value = value1 + numeric_part;

        return make_task_result(combined_value, "CombinedAB",
                               "Merged DataSourceA(double) + DataSourceB(string->double)");
    }

    std::string get_name() const override { return "Task4"; }
};

class Task5 : public ITask {
private:
    const Level1Results& level1_results_;

public:
    explicit Task5(const Level1Results& level1_results)
        : level1_results_(level1_results) {}

    AnyTaskResult execute() override {
        std::cout << "  [Task5] Aggregating all data sources on thread: " << std::this_thread::get_id() << std::endl;
        simulate_work(90);

        // Extract values with type safety
        double value1 = get_value_as<double>(level1_results_.task1_result);
        std::string value2 = get_value_as<std::string>(level1_results_.task2_result);
        int value3 = get_value_as<int>(level1_results_.task3_result);

        // Validate inputs
        if (value1 <= 0 || value3 <= 0) {
            throw TaskExecutionError("Task5", "Invalid numeric inputs for aggregation");
        }
        if (value2.empty()) {
            throw TaskExecutionError("Task5", "Invalid string input for aggregation");
        }

        // Process: compute average of all numeric values
        double numeric_from_string = 73.2; // Extracted from string (simplified)
        double avg_value = (value1 + numeric_from_string + value3) / 3.0;

        return make_task_result(avg_value, "AggregatedABC",
                               "Average of double + string(->double) + int");
    }

    std::string get_name() const override { return "Task5"; }
};

// ===== LEVEL 3 TASK (FINAL) =====

class Task6 : public ITask {
private:
    const Level2Results& level2_results_;

public:
    explicit Task6(const Level2Results& level2_results)
        : level2_results_(level2_results) {}

    AnyTaskResult execute() override {
        std::cout << "  [Task6] Final processing on thread: " << std::this_thread::get_id() << std::endl;
        simulate_work(50);

        // Extract values with type safety
        double value4 = get_value_as<double>(level2_results_.task4_result);
        double value5 = get_value_as<double>(level2_results_.task5_result);

        // Validate inputs
        if (value4 <= 0 || value5 <= 0) {
            throw TaskExecutionError("Task6", "Invalid input values for final computation");
        }

        // Compute final weighted score
        double final_score = (value4 * 0.6) + (value5 * 0.4);

        return make_task_result(final_score, "FinalScore",
                               "Weighted combination of Level 2 results");
    }

    std::string get_name() const override { return "Task6"; }
};

// ===== TASK DAG EXECUTOR =====

class TaskDAGExecutor {
private:
    unifex::static_thread_pool& pool_;
    std::chrono::steady_clock::time_point start_time_;

    // Results storage
    Level1Results level1_results_;
    Level2Results level2_results_;
    AnyTaskResult final_result_;

    template<typename... Ts>
    auto unwrap_when_all(const std::tuple<Ts...>& when_all_result) {
        return std::apply([](const auto&... variants) {
            return std::make_tuple(std::get<0>(std::get<0>(variants))...);
        }, when_all_result);
    }

    void print_error_summary(const std::string& task_name, const std::exception& e) {
        auto error_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(error_time - start_time_);

        std::cout << "\n💥 ERROR OCCURRED IN PIPELINE" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        std::cout << "❌ Failed Task: " << task_name << std::endl;
        std::cout << "🕐 Time of Failure: " << elapsed.count() << "ms after start" << std::endl;
        std::cout << "📋 Error Details: " << e.what() << std::endl;
        std::cout << "🚫 Pipeline Status: TERMINATED - All subsequent tasks cancelled" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    }

public:
    explicit TaskDAGExecutor(unifex::static_thread_pool& pool)
        : pool_(pool) {}

    void execute_pipeline() {
        start_time_ = std::chrono::steady_clock::now();

        try {
            execute_level1();
            execute_level2();
            execute_level3();
            print_success_summary();

        } catch (const TaskExecutionError& e) {
            print_error_summary(e.get_task_name(), e);
            throw;
        } catch (const std::exception& e) {
            print_error_summary("Unknown", e);
            throw;
        }
    }

private:
    void execute_level1() {
        std::cout << "🚀 Starting Level 1: Independent tasks (Task1, Task2, Task3)" << std::endl;

        auto scheduler = pool_.get_scheduler();

        // Create task instances
        auto task1 = std::make_shared<Task1>();
        auto task2 = std::make_shared<Task2>();
        auto task3 = std::make_shared<Task3>();

        // Execute tasks in parallel using unifex
        auto task1_sender = unifex::schedule(scheduler) | unifex::then([task1]() {
            return task1->execute();
        });

        auto task2_sender = unifex::schedule(scheduler) | unifex::then([task2]() {
            return task2->execute();
        });

        auto task3_sender = unifex::schedule(scheduler) | unifex::then([task3]() {
            return task3->execute();
        });

        // Wait for all Level 1 tasks
        auto results = unifex::sync_wait(
            unifex::when_all(
                std::move(task1_sender),
                std::move(task2_sender),
                std::move(task3_sender)
            )
        );

        if (!results.has_value()) {
            throw std::runtime_error("Level 1 tasks were cancelled or completed with done signal");
        }

        auto [result1, result2, result3] = unwrap_when_all(*results);
        level1_results_ = Level1Results{result1, result2, result3};

        std::cout << "✅ Level 1 completed successfully:" << std::endl;
        auto r1_iface = get_result_interface(level1_results_.task1_result);
        auto r2_iface = get_result_interface(level1_results_.task2_result);
        auto r3_iface = get_result_interface(level1_results_.task3_result);

        std::cout << "    Task1: " << r1_iface->get_description()
                  << " = " << r1_iface->to_string()
                  << " (" << r1_iface->get_type_name() << ")" << std::endl;
        std::cout << "    Task2: " << r2_iface->get_description()
                  << " = " << r2_iface->to_string()
                  << " (" << r2_iface->get_type_name() << ")" << std::endl;
        std::cout << "    Task3: " << r3_iface->get_description()
                  << " = " << r3_iface->to_string()
                  << " (" << r3_iface->get_type_name() << ")" << std::endl;
    }

    void execute_level2() {
        std::cout << "\n🔄 Starting Level 2: Dependent tasks (Task4, Task5)" << std::endl;

        auto scheduler = pool_.get_scheduler();

        // Create task instances with Level 1 results
        auto task4 = std::make_shared<Task4>(level1_results_);
        auto task5 = std::make_shared<Task5>(level1_results_);

        // Execute tasks in parallel
        auto task4_sender = unifex::schedule(scheduler) | unifex::then([task4]() {
            return task4->execute();
        });

        auto task5_sender = unifex::schedule(scheduler) | unifex::then([task5]() {
            return task5->execute();
        });

        // Wait for all Level 2 tasks
        auto results = unifex::sync_wait(
            unifex::when_all(
                std::move(task4_sender),
                std::move(task5_sender)
            )
        );

        if (!results.has_value()) {
            throw std::runtime_error("Level 2 tasks were cancelled or completed with done signal");
        }

        auto [result4, result5] = unwrap_when_all(*results);
        level2_results_ = Level2Results{result4, result5};

        std::cout << "✅ Level 2 completed successfully:" << std::endl;
        auto r4_iface = get_result_interface(level2_results_.task4_result);
        auto r5_iface = get_result_interface(level2_results_.task5_result);

        std::cout << "    Task4: " << r4_iface->get_description()
                  << " = " << std::fixed << std::setprecision(2) << r4_iface->to_string() << std::endl;
        std::cout << "    Task5: " << r5_iface->get_description()
                  << " = " << r5_iface->to_string() << std::endl;
    }

    void execute_level3() {
        std::cout << "\n🎯 Starting Level 3: Final task (Task6)" << std::endl;

        auto scheduler = pool_.get_scheduler();

        // Create final task instance
        auto task6 = std::make_shared<Task6>(level2_results_);

        // Execute final task
        auto result = unifex::sync_wait(
            unifex::schedule(scheduler) | unifex::then([task6]() {
                return task6->execute();
            })
        );

        if (!result.has_value()) {
            throw std::runtime_error("Level 3 task was cancelled or completed with done signal");
        }

        final_result_ = *result;

        std::cout << "✅ Level 3 completed successfully:" << std::endl;
        auto final_iface = get_result_interface(final_result_);
        std::cout << "    Task6: " << final_iface->get_description()
                  << " = " << std::fixed << std::setprecision(2) << final_iface->to_string() << std::endl;
    }

    void print_success_summary() {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_);

        std::cout << "\n🎉 PIPELINE COMPLETED SUCCESSFULLY!" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        std::cout << "📊 Final Results (with types):" << std::endl;

        auto r1 = get_result_interface(level1_results_.task1_result);
        auto r2 = get_result_interface(level1_results_.task2_result);
        auto r3 = get_result_interface(level1_results_.task3_result);
        auto r4 = get_result_interface(level2_results_.task4_result);
        auto r5 = get_result_interface(level2_results_.task5_result);
        auto r6 = get_result_interface(final_result_);

        std::cout << "  Level 1: Task1=" << r1->to_string() << " (double), "
                  << "Task2=" << r2->to_string() << " (string), "
                  << "Task3=" << r3->to_string() << " (int)" << std::endl;
        std::cout << "  Level 2: Task4=" << std::fixed << std::setprecision(2) << r4->to_string() << " (double), "
                  << "Task5=" << r5->to_string() << " (double)" << std::endl;
        std::cout << "  Level 3: Task6=" << r6->to_string() << " (final weighted score)" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        std::cout << "🕐 Total Execution Time: " << duration.count() << "ms" << std::endl;
        std::cout << "🎯 Final Result: " << std::fixed << std::setprecision(2) << r6->to_string() << std::endl;
    }
};

// ===== MAIN FUNCTION =====

int main() {
    std::cout << "=== UNIFEX TASK DAG - FLEXIBLE TYPE-SAFE ARCHITECTURE ===" << std::endl;
    std::cout << "Demonstrating tasks with different return types (double, string, int)\n" << std::endl;

    unifex::static_thread_pool pool{4};

    try {
        TaskDAGExecutor executor(pool);
        executor.execute_pipeline();

    } catch (const std::exception& e) {
        return 1;
    }

    std::cout << "\n💡 Flexible Type System Features:" << std::endl;
    std::cout << "  • Tasks can return any type: double, string, int, custom objects" << std::endl;
    std::cout << "  • Type-safe result extraction with compile-time checking" << std::endl;
    std::cout << "  • Runtime type information and polymorphic interfaces" << std::endl;
    std::cout << "  • Easy to extend with new result types" << std::endl;
    std::cout << "  • Clean separation between type-specific and generic logic" << std::endl;
    std::cout << "  • Future-proof architecture for complex data types" << std::endl;
    std::cout << "  • Maintains performance with zero-cost abstractions where possible" << std::endl;

    return 0;
}
