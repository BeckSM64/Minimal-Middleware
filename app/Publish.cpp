#include "MMW.h"

int main() {

    // Initialize library settings
    mmw_initialize("config.yml");

    // Create publishers
    mmw_create_publisher("Test Topic");

    // Publish test message
    mmw_publish("Test Topic", "This is a test message for Test Topic, best effort", MMW_BEST_EFFORT);
    mmw_publish("Test Topic", "This is a test message for Test Topic, reliable", MMW_RELIABLE);

    // Clean up publishers
    mmw_cleanup();
}
