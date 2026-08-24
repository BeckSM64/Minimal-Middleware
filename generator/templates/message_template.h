struct {{ message_name }} {
    {%- for key, message_field_lists in message_fields_dict.items() %}
        {%- for message_field_dict in message_field_lists %}
    {{ message_field_dict["type"] }} {{ message_field_dict["name"]}};
        {%- endfor %}
    {%- endfor %}
}
