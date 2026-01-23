#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include "MMW.h"

std::atomic<bool> g_running(true);

void signal_handler(int) {
    spdlog::info("Signal caught, stopping...");
    g_running = false;
}

void on_message(const char* topic, const char* message) {
    spdlog::info("[SUB] {} -> {}", topic, message);
}

int main() {
    std::signal(SIGINT, signal_handler);

    mmw_set_log_level(MMW_LOG_LEVEL_TRACE);

    if (mmw_initialize("127.0.0.1", 5000) != MMW_OK) {
        return 1;
    }

    // Subscribe
    mmw_create_subscriber("Test Topic", on_message);
    mmw_create_subscriber("Test Topic 2", on_message);

    spdlog::info("Subscriber running. Waiting for messages...");

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    spdlog::info("Deleting subscribers...");
    mmw_delete_subscriber("Test Topic");
    mmw_delete_subscriber("Test Topic 2");

    // Optional: still safe for global cleanup (serializer, maps, joins)
    mmw_cleanup();

    spdlog::info("Exit.");
    return 0;
}
