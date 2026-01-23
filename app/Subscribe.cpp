#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <thread>
#include <chrono>
#include <csignal>
#include "MMW.h"

// Global flag for clean exit
std::atomic<bool> g_running(true);

void signal_handler(int signal) {
    spdlog::info("Signal caught, stopping...");
    g_running = false;
}

void test_callback(const char* topic, const char* message) {
    spdlog::info("Got message in callback: {}", message);
    spdlog::info("Callback knows the topic is: {}", topic);
}

void test_callback_2(const char* topic, const char* message) {
    spdlog::info("Got message in callback 2: {}", message);
    spdlog::info("Callback knows the topic is: {}", topic);
}

int main() {
    // Setup signal handler for Ctrl+C
    std::signal(SIGINT, signal_handler);

    // Enable logging
    mmw_set_log_level(MMW_LOG_LEVEL_TRACE);

    // Initialize library
    mmw_initialize("127.0.0.1", 5000);

    // Create subscribers
    mmw_create_subscriber("Test Topic", test_callback);
    mmw_create_subscriber("Test Topic 2", test_callback_2);

    // Create publishers
    mmw_create_publisher("Test Topic");
    mmw_create_publisher("Test Topic 2");

    // Send some test messages
    for (int i = 0; i < 5; ++i) {
        mmw_publish("Test Topic", ("Hello " + std::to_string(i)).c_str(), MMW_BEST_EFFORT);
        mmw_publish("Test Topic 2", ("World " + std::to_string(i)).c_str(), MMW_BEST_EFFORT);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    spdlog::info("Entering main loop, press Ctrl+C to exit");

    // Keep main alive until signal
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    spdlog::info("Stopping subscribers and cleaning up...");
    mmw_cleanup();
    spdlog::info("Cleanup complete. Exiting.");
    return 0;
}
