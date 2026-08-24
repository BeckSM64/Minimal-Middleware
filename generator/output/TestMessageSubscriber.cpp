#include <MMW.h>
#include "TestMessageSubscriber.h"
#include "TestMessage.h"

TestMessageSubscriber::TestMessageSubscriber(void (*mmw_callback)(const char*, TestMessage*)) {
    mmw_create_subscriber_raw("Some Test Topic", mmw_callback);
}

TestMessageSubscriber::~TestMessageSubscriber() {
    mmw_delete_subscriber("Some Test Topic");
}