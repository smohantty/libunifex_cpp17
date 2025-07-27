#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <iomanip>
#include <tuple>
#include <unifex/static_thread_pool.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/just.hpp>
#include <unifex/then.hpp>
#include <unifex/when_all.hpp>
#include <unifex/scheduler_concepts.hpp>

/*
 * TASK DEPENDENCY GRAPH (DAG):
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
 * This demonstrates:
 * - Parallel execution of independent tasks
 * - Dependency coordination using when_all
 * - Multi-level dependency chains
 * - Efficient resource utilization
 */

template<typename... Ts>
auto unwrap_when_all(const std::tuple<Ts...>& when_all_result) {
    return std::apply([](const auto&... variants) {
        return std::make_tuple(std::get<0>(std::get<0>(variants))...);
    }, when_all_result);
}

int main() {
    std::cout << "=== UNIFEX TASK DEPENDENCY GRAPH (DAG) DEMO ===" << std::endl;
    std::cout << "Demonstrating complex task dependencies with when_all\n" << std::endl;

    unifex::static_thread_pool pool{4};
    auto scheduler = pool.get_scheduler();

    auto total_start = std::chrono::steady_clock::now();

    // ===== LEVEL 1: Independent Tasks (Parallel Execution) =====
    std::cout << "🚀 Starting Level 1: Independent tasks (Task1, Task2, Task3)" << std::endl;

    auto task1 = unifex::schedule(scheduler) | unifex::then([] {
        std::cout << "  [Task1] Processing data source A on thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 42.5;  // Using uniform double type to avoid complex nesting
    });

    auto task2 = unifex::schedule(scheduler) | unifex::then([] {
        std::cout << "  [Task2] Processing data source B on thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        return 73.2;
    });

    auto task3 = unifex::schedule(scheduler) | unifex::then([] {
        std::cout << "  [Task3] Processing data source C on thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        return 91.8;
    });

    // Wait for all Level 1 tasks to complete
    auto level1_results = unifex::sync_wait(
        unifex::when_all(
            std::move(task1),
            std::move(task2),
            std::move(task3)
        )
    );

    if (!level1_results.has_value()) {
        std::cout << "❌ Level 1 tasks failed!" << std::endl;
        return 1;
    }

    // Extract results - with uniform double types, still need to handle variant/tuple nesting
    auto [result1, result2, result3] = unwrap_when_all(*level1_results);

    std::cout << "✅ Level 1 completed:" << std::endl;
    std::cout << "    Task1: DataSourceA = " << std::fixed << std::setprecision(1) << result1 << std::endl;
    std::cout << "    Task2: DataSourceB = " << result2 << std::endl;
    std::cout << "    Task3: DataSourceC = " << result3 << std::endl;

    // Store values in regular variables for C++17 lambda capture compatibility
    double r1 = result1, r2 = result2, r3 = result3;

    // ===== LEVEL 2: Dependent Tasks =====
    std::cout << "\n🔄 Starting Level 2: Dependent tasks (Task4, Task5)" << std::endl;

    auto task4 = unifex::schedule(scheduler) | unifex::then([r1, r2] {
        std::cout << "  [Task4] Combining DataSourceA + DataSourceB on thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        return r1 + r2;  // Combined value
    });

    auto task5 = unifex::schedule(scheduler) | unifex::then([r1, r2, r3] {
        std::cout << "  [Task5] Aggregating all data sources on thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(90));
        return (r1 + r2 + r3) / 3.0;  // Average value
    });

    // Wait for Level 2 tasks to complete
    auto level2_results = unifex::sync_wait(
        unifex::when_all(
            std::move(task4),
            std::move(task5)
        )
    );

    if (!level2_results.has_value()) {
        std::cout << "❌ Level 2 tasks failed!" << std::endl;
        return 1;
    }

    auto [result4, result5] = unwrap_when_all(*level2_results);

    std::cout << "✅ Level 2 completed:" << std::endl;
    std::cout << "    Task4: CombinedAB = " << std::fixed << std::setprecision(2) << result4 << std::endl;
    std::cout << "    Task5: AggregatedABC = " << result5 << std::endl;

    // Store values in regular variables for C++17 lambda capture compatibility
    double r4 = result4, r5 = result5;

    // ===== LEVEL 3: Final Task =====
    std::cout << "\n🎯 Starting Level 3: Final task (Task6)" << std::endl;

    auto task6_result = unifex::sync_wait(
        unifex::schedule(scheduler) | unifex::then([r4, r5] {
            std::cout << "  [Task6] Final processing on thread: " << std::this_thread::get_id() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return (r4 * 0.6) + (r5 * 0.4);  // Weighted final score
        })
    );

    if (!task6_result.has_value()) {
        std::cout << "❌ Level 3 task failed!" << std::endl;
        return 1;
    }

    const double result6 = *task6_result;
    std::cout << "✅ Level 3 completed:" << std::endl;
    std::cout << "    Task6: FinalScore = " << std::fixed << std::setprecision(2) << result6 << std::endl;

    // ===== SUMMARY =====
    auto total_end = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start);

    std::cout << "\n📊 EXECUTION SUMMARY:" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << "Task Results:" << std::endl;
    std::cout << "  Level 1: Task1=" << result1 << ", Task2=" << result2 << ", Task3=" << result3 << std::endl;
    std::cout << "  Level 2: Task4=" << std::fixed << std::setprecision(2) << result4 << ", Task5=" << result5 << std::endl;
    std::cout << "  Level 3: Task6=" << result6 << " (final weighted score)" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << "🕐 Total Execution Time: " << total_duration.count() << "ms" << std::endl;
    std::cout << "🚀 Parallel Efficiency: Tasks executed in dependency order with maximum parallelism" << std::endl;
    std::cout << "🎯 Final Result: " << std::fixed << std::setprecision(2) << result6 << std::endl;

    std::cout << "\n💡 Implementation Notes:" << std::endl;
    std::cout << "  • Used uniform double return types to simplify when_all result handling" << std::endl;
    std::cout << "  • Each level waits for previous dependencies before proceeding" << std::endl;
    std::cout << "  • Maximum parallelism achieved within each dependency level" << std::endl;
    std::cout << "  • Demonstrates real-world async workflow orchestration patterns" << std::endl;
    std::cout << "  • Even with uniform types, when_all creates std::variant<std::tuple<T>> nesting" << std::endl;

    return 0;
}
