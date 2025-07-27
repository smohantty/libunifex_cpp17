#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <unifex/static_thread_pool.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/inline_scheduler.hpp>
#include <unifex/just.hpp>
#include <unifex/then.hpp>
#include <unifex/let_value.hpp>
// Note: With UNIFEX_NO_COROUTINES=1, we can safely include task.hpp
// but coroutine features will be disabled at compile time
#include <unifex/task.hpp>

namespace fs = std::filesystem;

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

    std::cout << "Application completed successfully!" << std::endl;
    return 0;
}