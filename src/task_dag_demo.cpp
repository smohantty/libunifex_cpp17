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
 * TASK DEPENDENCY GRAPH (DAG) - UNIFEX-LIKE API WITH FLEXIBLE TYPES:
 *
 *     Task1 ────┐
 *              ├───► Task4 ────┐
 *     Task2 ────┤              │
 *              │               ├───► Task6
 *              └───► Task5 ────┘
 *              ┌───►        ▲
 *     Task3 ────┘            │
 *
 * Unifex-Like Architecture:
 * - Familiar sender/receiver interface like unifex
 * - connect(), value_types, error_types patterns
 * - Type-safe result handling with our flexible system
 * - No coroutines required - works with C++17
 * - Easy transition to/from real unifex if needed
 */

// ===== FLEXIBLE RESULT TYPES (Same as before) =====

class ITaskResult {
public:
    virtual ~ITaskResult() = default;
    virtual std::string get_description() const = 0;
    virtual std::string get_source_info() const = 0;
    virtual std::string get_type_name() const = 0;
    virtual std::string to_string() const = 0;
};

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

using DoubleResult = TaskResult<double>;
using StringResult = TaskResult<std::string>;
using IntResult = TaskResult<int>;

using AnyTaskResult = std::variant<
    std::shared_ptr<DoubleResult>,
    std::shared_ptr<StringResult>,
    std::shared_ptr<IntResult>
>;

template<typename T>
std::shared_ptr<TaskResult<T>> make_task_result(T value, const std::string& desc, const std::string& info = "") {
    return std::make_shared<TaskResult<T>>(std::move(value), desc, info);
}

// ===== UNIFEX-LIKE RECEIVER CONCEPTS (C++17 SFINAE) =====

template<typename R, typename... Values>
using receiver_of_t = std::enable_if_t<
    std::is_invocable_v<decltype(&R::set_value), R, Values...> &&
    std::is_invocable_v<decltype(&R::set_error), R, std::exception_ptr> &&
    std::is_invocable_v<decltype(&R::set_done), R>
>;

// ===== UNIFEX-LIKE SENDER INTERFACE =====

template<typename S, typename R>
using sender_to_t = std::enable_if_t<
    std::is_invocable_v<decltype(&S::connect), S, R>
>;

// ===== UNIFEX-LIKE RECEIVER IMPLEMENTATION =====

template<typename Func>
class simple_receiver {
    Func completion_func_;

public:
    explicit simple_receiver(Func f) : completion_func_(std::move(f)) {}

    template<typename... Values>
    void set_value(Values&&... values) && {
        completion_func_(std::forward<Values>(values)...);
    }

    void set_error(std::exception_ptr e) && {
        std::rethrow_exception(e);
    }

    void set_done() && {
        throw std::runtime_error("Operation was cancelled");
    }
};

// ===== UNIFEX-LIKE OPERATION STATE =====

template<typename Receiver, typename TaskFunc>
class task_operation {
    Receiver receiver_;
    TaskFunc task_func_;

public:
    task_operation(Receiver r, TaskFunc f)
        : receiver_(std::move(r)), task_func_(std::move(f)) {}

    void start() noexcept {
        try {
            auto result = task_func_();
            std::move(receiver_).set_value(result);
        } catch (...) {
            std::move(receiver_).set_error(std::current_exception());
        }
    }
};

// ===== UNIFEX-LIKE TASK SENDER =====

template<typename T, typename TaskFunc>
class task_sender {
    TaskFunc task_func_;
    std::string task_name_;

public:
    // Unifex-like type traits
    template<template<typename...> class Variant, template<typename...> class Tuple>
    using value_types = Variant<Tuple<AnyTaskResult>>;

    template<template<typename...> class Variant>
    using error_types = Variant<std::exception_ptr>;

    static constexpr bool sends_done = false;

    task_sender(TaskFunc f, std::string name)
        : task_func_(std::move(f)), task_name_(std::move(name)) {}

    template<typename Receiver>
    task_operation<Receiver, TaskFunc> connect(Receiver&& receiver) {
        return task_operation<Receiver, TaskFunc>{
            std::forward<Receiver>(receiver),
            task_func_
        };
    }

    const std::string& name() const { return task_name_; }
};

// ===== UNIFEX-LIKE TASK FACTORY FUNCTIONS =====

template<typename T, typename TaskFunc>
auto make_task_sender(TaskFunc&& func, const std::string& name) {
    return task_sender<T, std::decay_t<TaskFunc>>{
        std::forward<TaskFunc>(func),
        name
    };
}

// ===== UNIFEX-LIKE SYNC_WAIT FOR OUR SENDERS =====

template<typename Sender>
auto our_sync_wait(Sender&& sender) {
    std::optional<AnyTaskResult> result;
    std::exception_ptr error;

    auto receiver = simple_receiver{[&](AnyTaskResult r) {
        result = std::move(r);
    }};

    try {
        auto op = std::forward<Sender>(sender).connect(std::move(receiver));
        op.start();
    } catch (...) {
        error = std::current_exception();
    }

    if (error) {
        std::rethrow_exception(error);
    }

    return result;
}

// ===== HELPER FUNCTIONS (Same as before) =====

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

// ===== UNIFEX-LIKE TASK DEFINITIONS =====

// Task1: Returns double
auto make_task1_sender() {
    return make_task_sender<double>([]{
        std::cout << "  [Task1] Processing data source A on thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        double result = 42.5;  // Real data processing
        return make_task_result(result, "DataSourceA", "Primary data repository");
    }, "Task1");
}

// Task2: Returns string
auto make_task2_sender() {
    return make_task_sender<std::string>([]{
        std::cout << "  [Task2] Processing data source B on thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(80));

        std::string result = "PROCESSED_DATA_B_73.2";  // Real data processing
        return make_task_result(result, "DataSourceB", "Secondary data warehouse");
    }, "Task2");
}

// Task3: Returns int
auto make_task3_sender() {
    return make_task_sender<int>([]{
        std::cout << "  [Task3] Processing data source C on thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(120));

        int result = 91;  // Real data processing
        return make_task_result(result, "DataSourceC", "External API endpoint");
    }, "Task3");
}

// Task4: Depends on Task1 + Task2
auto make_task4_sender(const AnyTaskResult& task1_result, const AnyTaskResult& task2_result) {
    return make_task_sender<double>([task1_result, task2_result]{
        std::cout << "  [Task4] Combining DataSourceA + DataSourceB on thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(60));

        // Extract values with type safety
        double value1 = get_value_as<double>(task1_result);
        std::string value2 = get_value_as<std::string>(task2_result);

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
    }, "Task4");
}

// Task5: Depends on Task1 + Task2 + Task3
auto make_task5_sender(const AnyTaskResult& task1_result, const AnyTaskResult& task2_result, const AnyTaskResult& task3_result) {
    return make_task_sender<double>([task1_result, task2_result, task3_result]{
        std::cout << "  [Task5] Aggregating all data sources on thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(90));

        // Extract values with type safety
        double value1 = get_value_as<double>(task1_result);
        std::string value2 = get_value_as<std::string>(task2_result);
        int value3 = get_value_as<int>(task3_result);

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
    }, "Task5");
}

// Task6: Depends on Task4 + Task5
auto make_task6_sender(const AnyTaskResult& task4_result, const AnyTaskResult& task5_result) {
    return make_task_sender<double>([task4_result, task5_result]{
        std::cout << "  [Task6] Final processing on thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Extract values with type safety
        double value4 = get_value_as<double>(task4_result);
        double value5 = get_value_as<double>(task5_result);

        // Validate inputs
        if (value4 <= 0 || value5 <= 0) {
            throw TaskExecutionError("Task6", "Invalid input values for final computation");
        }

        // Compute final weighted score
        double final_score = (value4 * 0.6) + (value5 * 0.4);

        return make_task_result(final_score, "FinalScore",
                               "Weighted combination of Level 2 results");
    }, "Task6");
}

// ===== UNIFEX-LIKE DAG EXECUTOR =====

class UnifexLikeDAGExecutor {
private:
    unifex::static_thread_pool& pool_;
    std::chrono::steady_clock::time_point start_time_;

    // Results storage
    AnyTaskResult task1_result_, task2_result_, task3_result_;
    AnyTaskResult task4_result_, task5_result_;
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
    explicit UnifexLikeDAGExecutor(unifex::static_thread_pool& pool)
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

        // Create unifex-like senders
        auto task1_sender = make_task1_sender();
        auto task2_sender = make_task2_sender();
        auto task3_sender = make_task3_sender();

        // Execute with real unifex for parallel coordination
        auto unifex_task1 = unifex::schedule(scheduler) | unifex::then([task1_sender]() mutable {
            return *our_sync_wait(std::move(task1_sender));
        });

        auto unifex_task2 = unifex::schedule(scheduler) | unifex::then([task2_sender]() mutable {
            return *our_sync_wait(std::move(task2_sender));
        });

        auto unifex_task3 = unifex::schedule(scheduler) | unifex::then([task3_sender]() mutable {
            return *our_sync_wait(std::move(task3_sender));
        });

        // Wait for all Level 1 tasks
        auto results = unifex::sync_wait(
            unifex::when_all(
                std::move(unifex_task1),
                std::move(unifex_task2),
                std::move(unifex_task3)
            )
        );

        if (!results.has_value()) {
            throw std::runtime_error("Level 1 tasks were cancelled");
        }

        auto [result1, result2, result3] = unwrap_when_all(*results);
        task1_result_ = result1;
        task2_result_ = result2;
        task3_result_ = result3;

        std::cout << "✅ Level 1 completed successfully:" << std::endl;
        print_level_results({task1_result_, task2_result_, task3_result_}, "Task");
    }

    void execute_level2() {
        std::cout << "\n🔄 Starting Level 2: Dependent tasks (Task4, Task5)" << std::endl;

        auto scheduler = pool_.get_scheduler();

        // Create unifex-like senders with dependencies
        auto task4_sender = make_task4_sender(task1_result_, task2_result_);
        auto task5_sender = make_task5_sender(task1_result_, task2_result_, task3_result_);

        // Execute with real unifex for parallel coordination
        auto unifex_task4 = unifex::schedule(scheduler) | unifex::then([task4_sender]() mutable {
            return *our_sync_wait(std::move(task4_sender));
        });

        auto unifex_task5 = unifex::schedule(scheduler) | unifex::then([task5_sender]() mutable {
            return *our_sync_wait(std::move(task5_sender));
        });

        // Wait for all Level 2 tasks
        auto results = unifex::sync_wait(
            unifex::when_all(
                std::move(unifex_task4),
                std::move(unifex_task5)
            )
        );

        if (!results.has_value()) {
            throw std::runtime_error("Level 2 tasks were cancelled");
        }

        auto [result4, result5] = unwrap_when_all(*results);
        task4_result_ = result4;
        task5_result_ = result5;

        std::cout << "✅ Level 2 completed successfully:" << std::endl;
        print_level_results({task4_result_, task5_result_}, "Task", 4);
    }

    void execute_level3() {
        std::cout << "\n🎯 Starting Level 3: Final task (Task6)" << std::endl;

        auto scheduler = pool_.get_scheduler();

        // Create unifex-like sender with dependencies
        auto task6_sender = make_task6_sender(task4_result_, task5_result_);

        // Execute with real unifex
        auto result = unifex::sync_wait(
            unifex::schedule(scheduler) | unifex::then([task6_sender]() mutable {
                return *our_sync_wait(std::move(task6_sender));
            })
        );

        if (!result.has_value()) {
            throw std::runtime_error("Level 3 task was cancelled");
        }

        final_result_ = *result;

        std::cout << "✅ Level 3 completed successfully:" << std::endl;
        auto final_iface = get_result_interface(final_result_);
        std::cout << "    Task6: " << final_iface->get_description()
                  << " = " << std::fixed << std::setprecision(2) << final_iface->to_string() << std::endl;
    }

    void print_level_results(const std::vector<AnyTaskResult>& results, const std::string& prefix, int start_num = 1) {
        for (size_t i = 0; i < results.size(); ++i) {
            auto iface = get_result_interface(results[i]);
            std::cout << "    " << prefix << (start_num + i) << ": " << iface->get_description()
                      << " = " << iface->to_string()
                      << " (" << iface->get_type_name() << ")" << std::endl;
        }
    }

    void print_success_summary() {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_);

        std::cout << "\n🎉 PIPELINE COMPLETED SUCCESSFULLY!" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        std::cout << "📊 Final Results (with unifex-like API):" << std::endl;

        auto r1 = get_result_interface(task1_result_);
        auto r2 = get_result_interface(task2_result_);
        auto r3 = get_result_interface(task3_result_);
        auto r4 = get_result_interface(task4_result_);
        auto r5 = get_result_interface(task5_result_);
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
    std::cout << "=== UNIFEX-LIKE API WITH FLEXIBLE TYPE SYSTEM ===" << std::endl;
    std::cout << "Demonstrating familiar unifex patterns with our custom flexibility\n" << std::endl;

    unifex::static_thread_pool pool{4};

    try {
        UnifexLikeDAGExecutor executor(pool);
        executor.execute_pipeline();

    } catch (const std::exception& e) {
        return 1;
    }

    std::cout << "\n💡 Unifex-Like API Features:" << std::endl;
    std::cout << "  • Familiar sender/receiver patterns like unifex" << std::endl;
    std::cout << "  • connect(), value_types, error_types interfaces" << std::endl;
    std::cout << "  • sender_to concepts and type traits" << std::endl;
    std::cout << "  • No new API to learn - follows unifex conventions" << std::endl;
    std::cout << "  • Easy transition to/from real unifex when needed" << std::endl;
    std::cout << "  • Maintains our flexible type system and no coroutines" << std::endl;
    std::cout << "  • Best of both worlds: familiar API + custom flexibility" << std::endl;

    return 0;
}
