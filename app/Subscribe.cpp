#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "MMW.h"

void test_callback(const char *message) {
    spdlog::info("Got message in callback: {}", message);
}

int main() {

    // Initialize library settings
    mmw_initialize("config.yml");

    // Test API call
    mmw_create_subscriber("Test Topic", test_callback);

    // Stay alive so subscriber stays up
    while (true) std::this_thread::sleep_for(std::chrono::seconds(1));

    // Cleanup
    mmw_cleanup();
}
