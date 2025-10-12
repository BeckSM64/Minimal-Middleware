#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>

#include "MMW.h"

#define PORT 5000
#define BUFFER_SIZE 1024

void test_callback(const char *message) {
    std::cout << "Got message in callback: " << message << std::endl;
}

int main() {

    // Test API call
    mmw_create_subscriber(test_callback);

    // Stay alive so subscriber stays up
    while (true) std::this_thread::sleep_for(std::chrono::seconds(1));

    // Cleanup
    mmw_cleanup();
}
