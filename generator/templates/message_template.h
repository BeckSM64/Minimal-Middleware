struct {{ struct_name }} {
    {%- for field in fields %}
    {{ field.type }} {{ field.name }};
    {%- endfor %}
};
