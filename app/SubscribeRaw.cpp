#include <string>
#include <chrono>
#include <thread>
#include <iostream>
#include "MMW.h"

typedef struct {
    char testString1[32];
    char testString2[32];
    char testString3[32];
    int testInt;
    long testLong;
    short testShort;
} TestRawMessageStruct;

void testRawMessageCallback(void* message) {
    TestRawMessageStruct *testRawMessageString = reinterpret_cast<TestRawMessageStruct*>(message);
    std::cout << testRawMessageString->testString1 << std::endl;
    std::cout << testRawMessageString->testString2 << std::endl;
    std::cout << testRawMessageString->testString3 << std::endl;
    std::cout << testRawMessageString->testInt << std::endl;
    std::cout << testRawMessageString->testLong << std::endl;
    std::cout << testRawMessageString->testShort << std::endl;
}

int main() {

    // Initialize library settings
    mmw_initialize("config.yml");

    mmw_create_subscriber_raw("Raw Message Topic", testRawMessageCallback);

    // Stay alive so subscriber stays up
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    mmw_cleanup();
}