#include <iostream>
#include <filesystem>
#include <unifex/task.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/inline_scheduler.hpp>

namespace fs = std::filesystem;

unifex::task<void> async_hello() {
    std::cout << "Hello from async task!" << std::endl;
    co_return;
}

unifex::task<int> async_calculation() {
    std::cout << "Performing async calculation..." << std::endl;
    co_return 42;
}

int main() {
    std::cout << "C++17 Application with libunifex" << std::endl;
    std::cout << "================================" << std::endl;

    // Demonstrate C++17 filesystem
    auto current_path = fs::current_path();
    std::cout << "Current directory: " << current_path << std::endl;

    // Demonstrate libunifex usage
    auto scheduler = unifex::inline_scheduler{};

    // Run async hello task
    unifex::sync_wait(async_hello());

    // Run async calculation task
    auto result = unifex::sync_wait(async_calculation());
    if (result.has_value()) {
        std::cout << "Async calculation result: " << *result << std::endl;
    }

    std::cout << "Application completed successfully!" << std::endl;
    return 0;
}