#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>
#include "MMW.h"

void test_callback(const char *topic, const char *message) {
    printf("Got message in callback: %s\n", message);
}

int main() {

    // Initialize library settings
    if (mmw_initialize("127.0.0.1", 5000) != MMW_OK) {
        return -1;
    }

    // Test API call
    if (mmw_create_subscriber("Test Topic", test_callback) != MMW_OK) {
        return -1;
    }

    // Sleep to keep subscriber up
    while (1) {
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
    }

    // Cleanup
    if (mmw_cleanup() != MMW_OK) {
        return -1;
    }
}
