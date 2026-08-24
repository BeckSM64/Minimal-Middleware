#pragma once
#include <cstdint>
{% for field in fields %}
    {%- if field.type in required_headers_dict.keys() %}
{{- required_headers_dict[field.type] }}
    {% endif %}
{%- endfor %}

struct {{ message_name }} {
    {%- for field in fields %}
    {{ cpp_type_dict[field.type] }} {{ field.name }};
    {%- endfor %}
};
