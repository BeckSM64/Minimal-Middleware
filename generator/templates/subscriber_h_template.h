#pragma once
#include <MMW.h>
#include "{{ message_name }}.h"

class {{ message_name }}Subscriber {
public:
    {{ message_name }}Subscriber(void (*mmw_callback)(const char*, {{ message_name }}*));
    ~{{ message_name }}Subscriber();
};
