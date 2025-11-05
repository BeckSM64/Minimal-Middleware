#include "MMW.h"

int main() {

    // Enable logging
    mmw_set_log_level(MMW_LOG_LEVEL_OFF);

    // Initialize library settings
    mmw_initialize("127.0.0.1", 5000);

    // Create publishers
    mmw_create_publisher("Test Topic");

    // Publish test message
    mmw_publish("Test Topic", "This is a test message for Test Topic, best effort", MMW_BEST_EFFORT);
    mmw_publish("Test Topic", "This is a test message for Test Topic, reliable", MMW_RELIABLE);

    // Clean up publishers
    mmw_cleanup();
}
