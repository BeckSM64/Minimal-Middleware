#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <iostream>

#include "MMW.h"

#define PORT 5000
#define BUFFER_SIZE 1024

int main() {

    // Initialize library settings
    mmw_initialize("config.yml");

    // Create publishers
    mmw_create_publisher("Test Topic");
    mmw_create_publisher("Test Topic 2");

    // Publish test message
    for (int i = 0; i < 50; i++) {
        mmw_publish("Test Topic", "This is a test message for Test Topic");
        mmw_publish("Test Topic 2", "This is a test message for Test Topic 2");
    }

    // Clean up publishers
    mmw_cleanup();
}
