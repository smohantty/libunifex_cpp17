#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <iomanip>
#include <tuple>
#include <stdexcept>
#include <random>
#include <memory>
#include <optional>
#include <unifex/static_thread_pool.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/just.hpp>
#include <unifex/then.hpp>
#include <unifex/when_all.hpp>
#include <unifex/scheduler_concepts.hpp>

/*
 * TASK DEPENDENCY GRAPH (DAG) WITH ERROR HANDLING - REFACTORED OOP VERSION:
 *
 *     Task1 ────┐
 *              ├───► Task4 ────┐
 *     Task2 ────┤              │
 *              │               ├───► Task6
 *              └───► Task5 ────┘
 *              ┌───►        ▲
 *     Task3 ────┘            │
 *
 * Refactored Architecture:
 * - Each task is a separate class with clear input/output interface
 * - TaskDAGExecutor coordinates the entire pipeline execution
 * - Maintains all error handling and parallel execution capabilities
 * - Clean separation of concerns and improved testability
 */

// ===== DATA TYPES =====

struct TaskResult {
    double value;
    std::string description;
    std::string source_info;

    TaskResult() : value(0.0), description(""), source_info("") {}
    TaskResult(double v, const std::string& desc, const std::string& info = "")
        : value(v), description(desc), source_info(info) {}
};

struct Level1Results {
    TaskResult task1_result;
    TaskResult task2_result;
    TaskResult task3_result;

    Level1Results() = default;
    Level1Results(const TaskResult& r1, const TaskResult& r2, const TaskResult& r3)
        : task1_result(r1), task2_result(r2), task3_result(r3) {}
};

struct Level2Results {
    TaskResult task4_result;
    TaskResult task5_result;

    Level2Results() = default;
    Level2Results(const TaskResult& r4, const TaskResult& r5)
        : task4_result(r4), task5_result(r5) {}
};

// ===== EXCEPTION TYPES =====

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

// ===== FAILURE CONFIGURATION =====

struct FailureConfig {
    bool task1_fail = false;
    bool task2_fail = false;
    bool task3_fail = false;
    bool task4_fail = false;
    bool task5_fail = false;
    bool task6_fail = false;

    bool enable_random_failures = false;
    double failure_probability = 0.2;
};

// ===== TASK INTERFACE =====

class ITask {
public:
    virtual ~ITask() = default;
    virtual TaskResult execute() = 0;
    virtual std::string get_name() const = 0;

protected:
    void simulate_work(int duration_ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    }

    void check_failure(const std::string& task_name, bool should_fail,
                      const FailureConfig& config, std::mt19937& gen,
                      std::uniform_real_distribution<>& dis) {
        if (should_fail || (config.enable_random_failures && dis(gen) < config.failure_probability)) {
            throw_specific_error(task_name);
        }
    }

    virtual void throw_specific_error(const std::string& task_name) = 0;
};

// ===== LEVEL 1 TASKS (INDEPENDENT) =====

class Task1 : public ITask {
private:
    const FailureConfig& config_;
    std::mt19937& gen_;
    std::uniform_real_distribution<>& dis_;

public:
    Task1(const FailureConfig& config, std::mt19937& gen, std::uniform_real_distribution<>& dis)
        : config_(config), gen_(gen), dis_(dis) {}

    TaskResult execute() override {
        std::cout << "  [Task1] Processing data source A on thread: " << std::this_thread::get_id() << std::endl;
        simulate_work(100);

        check_failure("Task1", config_.task1_fail, config_, gen_, dis_);

        return TaskResult(42.5, "DataSourceA", "Primary data repository");
    }

    std::string get_name() const override { return "Task1"; }

protected:
    void throw_specific_error(const std::string& task_name) override {
        throw DataValidationError(task_name, "DataSourceA contains corrupted records");
    }
};

class Task2 : public ITask {
private:
    const FailureConfig& config_;
    std::mt19937& gen_;
    std::uniform_real_distribution<>& dis_;

public:
    Task2(const FailureConfig& config, std::mt19937& gen, std::uniform_real_distribution<>& dis)
        : config_(config), gen_(gen), dis_(dis) {}

    TaskResult execute() override {
        std::cout << "  [Task2] Processing data source B on thread: " << std::this_thread::get_id() << std::endl;
        simulate_work(80);

        check_failure("Task2", config_.task2_fail, config_, gen_, dis_);

        return TaskResult(73.2, "DataSourceB", "Secondary data warehouse");
    }

    std::string get_name() const override { return "Task2"; }

protected:
    void throw_specific_error(const std::string& task_name) override {
        throw ResourceExhaustionError(task_name, "Database connection pool exhausted");
    }
};

class Task3 : public ITask {
private:
    const FailureConfig& config_;
    std::mt19937& gen_;
    std::uniform_real_distribution<>& dis_;

public:
    Task3(const FailureConfig& config, std::mt19937& gen, std::uniform_real_distribution<>& dis)
        : config_(config), gen_(gen), dis_(dis) {}

    TaskResult execute() override {
        std::cout << "  [Task3] Processing data source C on thread: " << std::this_thread::get_id() << std::endl;
        simulate_work(120);

        check_failure("Task3", config_.task3_fail, config_, gen_, dis_);

        return TaskResult(91.8, "DataSourceC", "External API endpoint");
    }

    std::string get_name() const override { return "Task3"; }

protected:
    void throw_specific_error(const std::string& task_name) override {
        throw TaskExecutionError(task_name, "Network timeout while fetching external API data");
    }
};

// ===== LEVEL 2 TASKS (DEPENDENT) =====

class Task4 : public ITask {
private:
    const Level1Results& level1_results_;
    const FailureConfig& config_;
    std::mt19937& gen_;
    std::uniform_real_distribution<>& dis_;

public:
    Task4(const Level1Results& level1_results, const FailureConfig& config,
          std::mt19937& gen, std::uniform_real_distribution<>& dis)
        : level1_results_(level1_results), config_(config), gen_(gen), dis_(dis) {}

    TaskResult execute() override {
        std::cout << "  [Task4] Combining DataSourceA + DataSourceB on thread: " << std::this_thread::get_id() << std::endl;
        simulate_work(60);

        check_failure("Task4", config_.task4_fail, config_, gen_, dis_);

        double combined_value = level1_results_.task1_result.value + level1_results_.task2_result.value;
        return TaskResult(combined_value, "CombinedAB",
                         "Merged " + level1_results_.task1_result.description + " + " + level1_results_.task2_result.description);
    }

    std::string get_name() const override { return "Task4"; }

protected:
    void throw_specific_error(const std::string& task_name) override {
        throw DataValidationError(task_name, "Result validation failed - combined value out of expected range");
    }
};

class Task5 : public ITask {
private:
    const Level1Results& level1_results_;
    const FailureConfig& config_;
    std::mt19937& gen_;
    std::uniform_real_distribution<>& dis_;

public:
    Task5(const Level1Results& level1_results, const FailureConfig& config,
          std::mt19937& gen, std::uniform_real_distribution<>& dis)
        : level1_results_(level1_results), config_(config), gen_(gen), dis_(dis) {}

    TaskResult execute() override {
        std::cout << "  [Task5] Aggregating all data sources on thread: " << std::this_thread::get_id() << std::endl;
        simulate_work(90);

        check_failure("Task5", config_.task5_fail, config_, gen_, dis_);

        double avg_value = (level1_results_.task1_result.value +
                           level1_results_.task2_result.value +
                           level1_results_.task3_result.value) / 3.0;
        return TaskResult(avg_value, "AggregatedABC", "Average of all three data sources");
    }

    std::string get_name() const override { return "Task5"; }

protected:
    void throw_specific_error(const std::string& task_name) override {
        throw ResourceExhaustionError(task_name, "Insufficient memory for aggregation operation");
    }
};

// ===== LEVEL 3 TASK (FINAL) =====

class Task6 : public ITask {
private:
    const Level2Results& level2_results_;
    const FailureConfig& config_;
    std::mt19937& gen_;
    std::uniform_real_distribution<>& dis_;

public:
    Task6(const Level2Results& level2_results, const FailureConfig& config,
          std::mt19937& gen, std::uniform_real_distribution<>& dis)
        : level2_results_(level2_results), config_(config), gen_(gen), dis_(dis) {}

    TaskResult execute() override {
        std::cout << "  [Task6] Final processing on thread: " << std::this_thread::get_id() << std::endl;
        simulate_work(50);

        check_failure("Task6", config_.task6_fail, config_, gen_, dis_);

        double final_score = (level2_results_.task4_result.value * 0.6) +
                            (level2_results_.task5_result.value * 0.4);
        return TaskResult(final_score, "FinalScore", "Weighted combination of Level 2 results");
    }

    std::string get_name() const override { return "Task6"; }

protected:
    void throw_specific_error(const std::string& task_name) override {
        throw TaskExecutionError(task_name, "Final validation failed - output format incompatible");
    }
};

// ===== TASK DAG EXECUTOR =====

class TaskDAGExecutor {
private:
    unifex::static_thread_pool& pool_;
    const FailureConfig& config_;
    std::mt19937& gen_;
    std::uniform_real_distribution<>& dis_;
    std::chrono::steady_clock::time_point start_time_;

    // Results storage
    Level1Results level1_results_;
    Level2Results level2_results_;
    TaskResult final_result_;

    template<typename... Ts>
    auto unwrap_when_all(const std::tuple<Ts...>& when_all_result) {
        return std::apply([](const auto&... variants) {
            return std::make_tuple(std::get<0>(std::get<0>(variants))...);
        }, when_all_result);
    }

    void print_error_summary(const std::string& level, const std::string& task_name,
                            const std::exception& e) {
        auto error_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(error_time - start_time_);

        std::cout << "\n💥 ERROR OCCURRED IN " << level << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        std::cout << "❌ Failed Task: " << task_name << std::endl;
        std::cout << "🕐 Time of Failure: " << elapsed.count() << "ms after start" << std::endl;
        std::cout << "📋 Error Details: " << e.what() << std::endl;
        std::cout << "🚫 Pipeline Status: TERMINATED - All subsequent tasks cancelled" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    }

public:
    TaskDAGExecutor(unifex::static_thread_pool& pool, const FailureConfig& config,
                   std::mt19937& gen, std::uniform_real_distribution<>& dis)
        : pool_(pool), config_(config), gen_(gen), dis_(dis) {}

    void execute_pipeline() {
        start_time_ = std::chrono::steady_clock::now();

        try {
            execute_level1();
            execute_level2();
            execute_level3();
            print_success_summary();

        } catch (const DataValidationError& e) {
            print_error_summary("DATA VALIDATION", e.get_task_name(), e);
            throw;
        } catch (const ResourceExhaustionError& e) {
            print_error_summary("RESOURCE MANAGEMENT", e.get_task_name(), e);
            throw;
        } catch (const TaskExecutionError& e) {
            print_error_summary("TASK EXECUTION", e.get_task_name(), e);
            throw;
        } catch (const std::exception& e) {
            print_error_summary("UNKNOWN ERROR", "Unknown", e);
            throw;
        }
    }

private:
    void execute_level1() {
        std::cout << "🚀 Starting Level 1: Independent tasks (Task1, Task2, Task3)" << std::endl;

        auto scheduler = pool_.get_scheduler();

        // Create task instances
        auto task1 = std::make_shared<Task1>(config_, gen_, dis_);
        auto task2 = std::make_shared<Task2>(config_, gen_, dis_);
        auto task3 = std::make_shared<Task3>(config_, gen_, dis_);

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
        std::cout << "    Task1: " << level1_results_.task1_result.description
                  << " = " << std::fixed << std::setprecision(1) << level1_results_.task1_result.value << std::endl;
        std::cout << "    Task2: " << level1_results_.task2_result.description
                  << " = " << level1_results_.task2_result.value << std::endl;
        std::cout << "    Task3: " << level1_results_.task3_result.description
                  << " = " << level1_results_.task3_result.value << std::endl;
    }

    void execute_level2() {
        std::cout << "\n🔄 Starting Level 2: Dependent tasks (Task4, Task5)" << std::endl;

        auto scheduler = pool_.get_scheduler();

        // Create task instances with Level 1 results
        auto task4 = std::make_shared<Task4>(level1_results_, config_, gen_, dis_);
        auto task5 = std::make_shared<Task5>(level1_results_, config_, gen_, dis_);

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
        std::cout << "    Task4: " << level2_results_.task4_result.description
                  << " = " << std::fixed << std::setprecision(2) << level2_results_.task4_result.value << std::endl;
        std::cout << "    Task5: " << level2_results_.task5_result.description
                  << " = " << level2_results_.task5_result.value << std::endl;
    }

    void execute_level3() {
        std::cout << "\n🎯 Starting Level 3: Final task (Task6)" << std::endl;

        auto scheduler = pool_.get_scheduler();

        // Create final task instance
        auto task6 = std::make_shared<Task6>(level2_results_, config_, gen_, dis_);

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
        std::cout << "    Task6: " << final_result_.description
                  << " = " << std::fixed << std::setprecision(2) << final_result_.value << std::endl;
    }

    void print_success_summary() {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_);

        std::cout << "\n🎉 PIPELINE COMPLETED SUCCESSFULLY!" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        std::cout << "📊 Final Results:" << std::endl;
        std::cout << "  Level 1: Task1=" << std::fixed << std::setprecision(1) << level1_results_.task1_result.value
                  << ", Task2=" << level1_results_.task2_result.value
                  << ", Task3=" << level1_results_.task3_result.value << std::endl;
        std::cout << "  Level 2: Task4=" << std::fixed << std::setprecision(2) << level2_results_.task4_result.value
                  << ", Task5=" << level2_results_.task5_result.value << std::endl;
        std::cout << "  Level 3: Task6=" << final_result_.value << " (final weighted score)" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        std::cout << "🕐 Total Execution Time: " << duration.count() << "ms" << std::endl;
        std::cout << "🎯 Final Result: " << std::fixed << std::setprecision(2) << final_result_.value << std::endl;
    }
};

// ===== MAIN FUNCTION =====

int main() {
    std::cout << "=== UNIFEX TASK DAG WITH OOP REFACTOR & ERROR HANDLING ===" << std::endl;
    std::cout << "Demonstrating modular task classes with coordinated execution\n" << std::endl;

    // Configure failure scenarios
    FailureConfig config;
    // Uncomment to test specific failures:
    // config.task2_fail = true;  // Test Level 1 failure
    // config.task5_fail = true;  // Test Level 2 failure
    // config.task6_fail = true;  // Test Level 3 failure
    config.enable_random_failures = false;  // Enable random failures for realistic testing

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    unifex::static_thread_pool pool{4};

    try {
        TaskDAGExecutor executor(pool, config, gen, dis);
        executor.execute_pipeline();

    } catch (const std::exception& e) {
        return 1;
    }

    std::cout << "\n💡 OOP Refactor Features:" << std::endl;
    std::cout << "  • Separate task classes with clear input/output interfaces" << std::endl;
    std::cout << "  • TaskDAGExecutor coordinates the entire pipeline execution" << std::endl;
    std::cout << "  • Maintains all error handling and parallel execution capabilities" << std::endl;
    std::cout << "  • Clean separation of concerns improves testability and maintainability" << std::endl;
    std::cout << "  • Each task can be independently tested and reused" << std::endl;
    std::cout << "  • Executor handles unifex coordination and error propagation" << std::endl;

    return 0;
}
