#include "TestMessageSubscriber.h"
#include <iostream>

void testRawMessageCallback(const char* topic, TestMessage* message) {
    std::cout << "GOT CALLBACK" << std::endl;
}

int main() {
    MmwResult result = mmw_initialize("127.0.0.1", 5000);

    if (result != MMW_OK) {
        std::cout << "Failed to connect to the broker... exiting" << std::endl;
        return -1;
    }

    TestMessageSubscriber *subscriber = new TestMessageSubscriber(testRawMessageCallback);
    TestMessage message{
        false,                               // testBool
        0,                                   // testByte
        0,                                   // testUnsignedByte
        10,                                  // testShort
        20,                                  // testUnsignedShort
        30,                                  // testInt
        40,                                  // testUnsignedInt
        50LL,                                // testLong
        60ULL,                               // testUnsignedLong
        1.1f,                                // testFloat
        2.2,                                 // testDouble
        "Fallback String",                   // testString
        std::vector<uint8_t>{1, 2, 3, 4}     // testVector
    };
    std::cout << result << std::endl;
}
