#include <unistd.h>
#include <stdio.h>
#include "MMW.h"

void test_callback(const char *message) {
    printf("Got message in callback: %s\n", message);
}

int main() {

    // Initialize library settings
    mmw_initialize("config.yml");

    // Test API call
    mmw_create_subscriber("Test Topic", test_callback);

    // Sleep to keep subscriber up
    while (1) {
        sleep(1);
    }

    // Cleanup
    mmw_cleanup();
}
