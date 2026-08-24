#include <MMW.h>
#include "{{ message_name }}.h"

class {{ message_name }}Publisher {
    {{ message_name }}Publisher();
    ~{{ message_name }}Publisher();
    MmwResult Publish({{ message_name }} message);
};
