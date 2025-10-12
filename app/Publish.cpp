#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

#include "MMW.h"

#define PORT 5000
#define BUFFER_SIZE 1024

int main() {

    // Create publishers
    mmw_create_publisher("Test Topic");
    mmw_create_publisher("Test Topic 2");

    // Test publish calls for test topic 1
    mmw_publish("Test Topic", "This is a test message for Test Topic\n");
    mmw_publish("Test Topic", "This is a second test message for Test Topic\n");

    // Test publish calls for test topic 2
    mmw_publish("Test Topic 2", "This is a test message for Test Topic 2\n");
    mmw_publish("Test Topic 2", "This is a second test message for Test Topic 2\n");

    // Clean up publishers
    mmw_cleanup();
}
