#include <MMW.h>
#include "TestMessagePublisher.h"
#include "TestMessage.h"

TestMessagePublisher::TestMessagePublisher() {
    mmw_create_publisher("Some Test Topic");
}

TestMessagePublisher::~TestMessagePublisher() {
    mmw_delete_publisher("Some Test Topic");
}

MmwResult TestMessagePublisher::Publish(TestMessage message) {
    MmwResult result = mmw_publish_raw("Some Test Topic", &message, sizeof(message), MMW_RELIABLE);
    return result;
}