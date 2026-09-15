#include "MMW.h"
#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

void publisher_loop() {
    static int ticks = 0;

    if (++ticks < 10) {
        return;
    }

    if (mmw_delete_publisher("Test Topic") != MMW_OK) {
        spdlog::error("Failed to delete MMW publisher");
    }

    if (mmw_delete_publisher("Test Topic 2") != MMW_OK) {
        spdlog::error("Failed to delete MMW publisher");
    }

    mmw_cleanup();

    emscripten_cancel_main_loop();
}

#endif

int main() {

    // Enable logging
    mmw_set_log_level(MMW_LOG_LEVEL_INFO);

    // Initialize library
    if (mmw_initialize(
            "127.0.0.1",
            5001,
            MMW_TRANSPORT_WEBSOCKET) != MMW_OK) {
        spdlog::error("Failed to initialize MMW");
        return 1;
    }

    // Create publishers
    if (mmw_create_publisher("Test Topic") != MMW_OK) {
        spdlog::error("Failed to create MMW publisher");
        return -1;
    }

    if (mmw_create_publisher("Test Topic 2") != MMW_OK) {
        spdlog::error("Failed to create MMW publisher");
        return -1;
    }

    // Publish messages
    if (mmw_publish(
            "Test Topic",
            "This is a test message for Test Topic, reliable",
            MMW_RELIABLE) != MMW_OK) {
        spdlog::error("Failed to publish MMW message");
        return -1;
    }

    if (mmw_publish(
            "Test Topic 2",
            "This is a test message for Test Topic 2, reliable",
            MMW_RELIABLE) != MMW_OK) {
        spdlog::error("Failed to publish MMW message");
        return -1;
    }

#ifdef __EMSCRIPTEN__

    emscripten_set_main_loop(publisher_loop, 0, 1);

#else

    // Give broker time to process
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100)
    );

    // Test per-publisher cleanup
    if (mmw_delete_publisher("Test Topic") != MMW_OK) {
        spdlog::error("Failed to delete MMW publisher");
        return -1;
    }

    if (mmw_delete_publisher("Test Topic 2") != MMW_OK) {
        spdlog::error("Failed to delete MMW publisher");
        return -1;
    }

#endif

    return 0;
}