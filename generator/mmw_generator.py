#!/usr/bin/env python3
import yaml
from jinja2 import Environment, FileSystemLoader
from pathlib import Path

cpp_type_dict = {
    "bool": "bool",
    "int8": "int8_t",
    "uint8": "uint8_t",
    "int16": "int16_t",
    "uint16": "uint16_t",
    "int32": "int32_t",
    "uint32": "uint32_t",
    "int64": "int64_t",
    "uint64": "uint64_t",
    "float32": "float",
    "float": "float",
    "float64": "double",
    "double": "double",
    "string": "std::string",
    "bytes": "std::vector<uint8_t>"
}

required_headers_dict = {
    "string" : "#include <string>",
    "bytes" : "#include <vector>"
}

def generate_template(template, output_name, msg_name, msg_data):
    print("Generating message header")

    # Render template
    output = template.render(
        message_name=msg_name, 
        fields=msg_data.get("fields", []),
        required_headers_dict=required_headers_dict,
        cpp_type_dict=cpp_type_dict
    )

    # Save the generated files
    output_path = f"output/{output_name}"
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(output)
        
    print(f"Generated: {output_path}")


def main():
    print("MMW Generator Started...")
    
    # initialize jinja environment
    env = Environment(loader=FileSystemLoader("templates/"))

    # initialize the templates
    message_template = env.get_template("message_template.h")
    publisher_header_template = env.get_template("publisher_h_template.h")
    publisher_cpp_template = env.get_template("publisher_cpp_template.h")
    subscriber_header_template = env.get_template("subscriber_h_template.h")
    subscriber_cpp_template = env.get_template("subscriber_cpp_template.h")

    # parse the yml idls
    idls = [p for p in Path('../idls/').iterdir() if p.is_file()]

    # generate headers for each individual idl
    for idl in idls:

        # need this for .gitkeep and any other non idl files
        if idl.suffix == ".yml":
            with open(idl, "r") as file:
                messages_dict = yaml.safe_load(file)

            # Build the output files
            for msg_name, msg_data in messages_dict.items():

                # generate the message header
                generate_template(message_template, f"{msg_name}.h", msg_name, msg_data)

                # generate the publisher header
                generate_template(publisher_header_template, f"{msg_name}Publisher.h", msg_name, msg_data)

                # generate the publisher cpp
                generate_template(publisher_cpp_template, f"{msg_name}Publisher.cpp", msg_name, msg_data)

                # generate the subscriber header
                generate_template(subscriber_header_template, f"{msg_name}Subscriber.h", msg_name, msg_data)

                # generate the subscriber cpp
                generate_template(subscriber_cpp_template, f"{msg_name}Subscriber.cpp", msg_name, msg_data)


if __name__ == "__main__":
    main()
