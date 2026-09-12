#include <cstring>
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

int main() {

    TestRawMessageStruct testRawMessageStruct = {};
    strncpy(testRawMessageStruct.testString1, "hello1", sizeof(testRawMessageStruct.testString1) - 1);
    strncpy(testRawMessageStruct.testString2, "hello2", sizeof(testRawMessageStruct.testString2) - 1);
    strncpy(testRawMessageStruct.testString3, "hello3", sizeof(testRawMessageStruct.testString3) - 1);
    testRawMessageStruct.testInt = 103456;
    testRawMessageStruct.testLong = 90394580;
    testRawMessageStruct.testShort = 10;

    // Initialize library settings
    if (mmw_initialize("127.0.0.1", 5000) != MMW_OK) {
        spdlog::error("Failed to initialize MMW");
        return -1;
    }

    // Create publisher
    if (mmw_create_publisher("Raw Message Topic") != MMW_OK) {
        spdlog::error("Failed to create MMW publisher");
        return -1;
    }

    // Publish test messages
    for (int i = 0; i < 10000; i++) {
        if (mmw_publish_raw("Raw Message Topic", &testRawMessageStruct, sizeof(testRawMessageStruct), MMW_RELIABLE) != MMW_OK) {
            spdlog::error("Failed to publish MMW message");
        }
    }

    // Cleanup mmw resources
    if (mmw_cleanup() != MMW_OK) {
        spdlog::error("Failed to cleanup MMW resources");
        return -1;
    }
}
