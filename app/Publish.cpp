#include "MMW.h"
#include <thread>
#include <chrono>

int main() {

    // Enable logging
    mmw_set_log_level(MMW_LOG_LEVEL_INFO);

    // Initialize library
    if (mmw_initialize("127.0.0.1", 5000) != MMW_OK) {
        return 1;
    }

    // Create publishers
    mmw_create_publisher("Test Topic");
    mmw_create_publisher("Test Topic 2");

    // Publish messages
    mmw_publish("Test Topic",
                "This is a test message for Test Topic, reliable",
                MMW_RELIABLE);

    mmw_publish("Test Topic 2",
                "This is a test message for Test Topic 2, reliable",
                MMW_RELIABLE);

    // Give broker time to process (important for unregister test)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Test per-publisher cleanup
    mmw_delete_publisher("Test Topic");
    mmw_delete_publisher("Test Topic 2");

    // Final cleanup (should be fast + no hangs)
    mmw_cleanup();
    return 0;
}
