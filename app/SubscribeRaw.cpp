#include <string>
#include <csignal>
#include <spdlog/spdlog.h>
#include "MMW.h"

void signal_handler(int) {
    spdlog::info("Signal caught, stopping...");
    mmw_stop();
}

typedef struct {
    char testString1[32];
    char testString2[32];
    char testString3[32];
    int32_t testInt;
    int64_t testLong;
    int16_t testShort;
} TestRawMessageStruct;

void testRawMessageCallback(const char* topic, void* message) {
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

    std::signal(SIGINT, signal_handler);

    mmw_set_log_level(MMW_LOG_LEVEL_TRACE);

    // Initialize library settings
    if (mmw_initialize("127.0.0.1", 5001, MMW_TRANSPORT_WEBSOCKET) != MMW_OK) {
        spdlog::error("Failed to initialize MMW");
        return -1;
    }

    // Create subscriber
    if (mmw_create_subscriber_raw("Raw Message Topic", testRawMessageCallback) != MMW_OK) {
        spdlog::error("Failed to create MMW subscriber");
        return -1;
    }

    // Stay alive so subscriber stays up
    mmw_wait();

    if (mmw_cleanup() != MMW_OK) {
        spdlog::error("Failed to cleanup MMW resources");
        return -1;
    }

}
