#include <spdlog/spdlog.h>
#include <csignal>
#include "MMW.h"

void signal_handler(int) {
    spdlog::info("Signal caught, stopping...");
    mmw_stop();
}

void on_message(const char* topic, const char* message) {
    spdlog::info("[SUB] {} -> {}", topic, message);
}

int main() {
    std::signal(SIGINT, signal_handler);

    mmw_set_log_level(MMW_LOG_LEVEL_TRACE);

    if (mmw_initialize(
            "127.0.0.1",
            5001,
            MMW_TRANSPORT_WEBSOCKET) != MMW_OK) {
        return -1;
    }

    if (mmw_create_subscriber("Test Topic", on_message) != MMW_OK) {
        return -1;
    }

    if (mmw_create_subscriber("Test Topic 2", on_message) != MMW_OK) {
        return -1;
    }

    spdlog::info("Subscriber running. Waiting for messages...");

    mmw_wait();

    mmw_delete_subscriber("Test Topic");
    mmw_delete_subscriber("Test Topic 2");
    mmw_cleanup();

    spdlog::info("Exit.");

    return 0;
}
