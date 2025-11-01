#include <string>
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>
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
    spdlog::info("{} {} {} {} {} {}", 
        testRawMessageString->testString1,
        testRawMessageString->testString2,
        testRawMessageString->testString3,
        testRawMessageString->testInt,
        testRawMessageString->testLong,
        testRawMessageString->testShort);
}

int main() {

    // Initialize library settings
    mmw_initialize("127.0.0.1", 5000);

    mmw_create_subscriber_raw("Raw Message Topic", testRawMessageCallback);

    // Stay alive so subscriber stays up
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    mmw_cleanup();
}