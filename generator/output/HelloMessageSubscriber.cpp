#include <MMW.h>
#include "HelloMessageSubscriber.h"
#include "HelloMessage.h"

HelloMessageSubscriber::HelloMessageSubscriber(void (*mmw_callback)(const char*, HelloMessage*)) {
    mmw_create_subscriber_raw("Some Test Topic", mmw_callback);
}

HelloMessageSubscriber::~HelloMessageSubscriber() {
    mmw_delete_subscriber("Some Test Topic");
}