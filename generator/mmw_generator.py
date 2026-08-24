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

def main():
    print("MMW Generator Started...")
    
    # Initialize Jinja environment
    env = Environment(loader=FileSystemLoader("templates"))
    template = env.get_template("message_template.h")

    # Parse the yml idls
    idls = [p for p in Path('../idls/').iterdir() if p.is_file()]

    # Generate headers for each individual idl
    for idl in idls:

        # Need this for .gitkeep and any other non idl files
        if idl.suffix == ".yml":
            with open(idl, "r") as file:
                messages_dict = yaml.safe_load(file)

            # Build the output files
            for msg_name, msg_data in messages_dict.items():

                # Render template
                output = template.render(
                    struct_name=msg_name, 
                    fields=msg_data.get("fields", []),
                    required_headers_dict=required_headers_dict,
                    cpp_type_dict=cpp_type_dict
                )

                # Save the generated files
                output_path = f"output/{msg_name}.h"
                with open(output_path, "w", encoding="utf-8") as f:
                    f.write(output)
                    
                print(f"Generated: {output_path}")

if __name__ == "__main__":
    main()
