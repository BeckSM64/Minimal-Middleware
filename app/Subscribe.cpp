#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "MMW.h"

void test_callback(const char *message) {
    spdlog::info("Got message in callback: {}", message);
}

int main() {

    // Enable logging
    mmw_set_log_level(MMW_LOG_LEVEL_OFF);

    // Initialize library settings
    mmw_initialize("127.0.0.1", 5000);

    // Test API call
    mmw_create_subscriber("Test Topic", test_callback);

    // Stay alive so subscriber stays up
    while (true) std::this_thread::sleep_for(std::chrono::seconds(1));

    // Cleanup
    mmw_cleanup();
}
