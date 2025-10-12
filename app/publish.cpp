#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

#include "MMW.h"

#define PORT 5000
#define BUFFER_SIZE 1024

int main() {

    // Test API call
    mmw_create_publisher();
    mmw_publish("This is a test message");
    mmw_cleanup();
}
