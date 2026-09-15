#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include "MMW.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif


std::atomic<bool> g_running(true);

void signal_handler(int) {
    spdlog::info("Signal caught, stopping...");
    g_running = false;
}

void on_message(const char* topic, const char* message) {
    spdlog::info("[SUB] {} -> {}", topic, message);
}

#ifdef __EMSCRIPTEN__

void main_loop() {
    if (!g_running) {
        emscripten_cancel_main_loop();

        spdlog::info("Deleting subscribers...");

        mmw_delete_subscriber("Test Topic");
        mmw_delete_subscriber("Test Topic 2");
        mmw_cleanup();

        spdlog::info("Exit.");
    }
}

#endif

int main() {
    std::signal(SIGINT, signal_handler);

    mmw_set_log_level(MMW_LOG_LEVEL_TRACE);

    if (mmw_initialize(
            "127.0.0.1",
            5001,
            MMW_TRANSPORT_WEBSOCKET) != MMW_OK) {
        spdlog::error("Failed to initialize MMW");
        return -1;
    }

    if (mmw_create_subscriber("Test Topic", on_message) != MMW_OK) {
        spdlog::error("Failed to create MMW subscriber");
        return -1;
    }

    if (mmw_create_subscriber("Test Topic 2", on_message) != MMW_OK) {
        spdlog::error("Failed to create MMW subscriber");
        return -1;
    }

    spdlog::info("Subscriber running. Waiting for messages...");

#ifdef __EMSCRIPTEN__

    emscripten_set_main_loop(main_loop, 0, 1);

#else

    while (g_running) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(200)
        );
    }

    spdlog::info("Deleting subscribers...");

    if (mmw_delete_subscriber("Test Topic") != MMW_OK) {
        spdlog::error("Failed to delete MMW subscriber");
        return -1;
    }

    if (mmw_delete_subscriber("Test Topic 2") != MMW_OK) {
        spdlog::error("Failed to delete MMW subscriber");
        return -1;
    }

    if (mmw_cleanup() != MMW_OK) {
        spdlog::error("Failed to cleanup MMW resources");
        return -1;
    }

    spdlog::info("Exit.");

#endif

    return 0;
}