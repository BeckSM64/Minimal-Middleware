#include <MMW.h>
#include "{{ message_name }}Subscriber.h"
#include "{{ message_name }}.h"

{{ message_name }}Subscriber::{{ message_name }}Subscriber(void (*mmw_callback)(const char*, {{ message_name }}*)) {
    mmw_create_subscriber_raw("Some Test Topic", mmw_callback);
}

{{ message_name }}Subscriber::~{{ message_name }}Subscriber() {
    mmw_delete_subscriber("Some Test Topic");
}
