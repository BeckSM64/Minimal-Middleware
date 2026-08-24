#include <MMW.h>
#include "{{ message_name }}Publisher.h"
#include "{{ message_name }}.h"

{{ message_name }}Publisher::{{ message_name }}Publisher() {
    mmw_create_publisher("Some Test Topic");
}

{{ message_name }}Publisher::~{{ message_name }}Publisher() {
    mmw_delete_publisher("Some Test Topic");
}

MmwResult {{ message_name }}Publisher::Publish({{ message_name }} message) {
    MmwResult result = mmw_publish_raw("Some Test Topic", &message, sizeof(message), MMW_RELIABLE);
    return result;
}