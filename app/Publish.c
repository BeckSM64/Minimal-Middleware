#include "MMW.h"

int main() {

    // Initialize library settings
    if (mmw_initialize("127.0.0.1", 5000) != MMW_OK) {
        return -1;
    }

    // Create publishers
    if (mmw_create_publisher("Test Topic") != MMW_OK) {
        return -1;
    }

    // Publish test message
    if (mmw_publish("Test Topic", "This was published by the publish_c sample application", MMW_BEST_EFFORT) != MMW_OK) {
        return -1;
    }

    // Clean up publishers
    if (mmw_cleanup() != MMW_OK) {
        return -1;
    }
}
