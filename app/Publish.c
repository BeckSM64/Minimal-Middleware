#include "MMW.h"

int main() {

    // Initialize library settings
    mmw_initialize("config.yml");

    // Create publishers
    mmw_create_publisher("Test Topic");

    // Publish test message
    mmw_publish("Test Topic", "This was published by the publish_c sample application");

    // Clean up publishers
    mmw_cleanup();
}
