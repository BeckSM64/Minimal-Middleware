#include <MMW.h>
#include "HelloMessagePublisher.h"
#include "HelloMessage.h"

HelloMessagePublisher::HelloMessagePublisher() {
    mmw_create_publisher("Some Test Topic");
}

HelloMessagePublisher::~HelloMessagePublisher() {
    mmw_delete_publisher("Some Test Topic");
}

MmwResult HelloMessagePublisher::Publish(HelloMessage message) {
    MmwResult result = mmw_publish_raw("Some Test Topic", &message, sizeof(message), MMW_RELIABLE);
    return result;
}