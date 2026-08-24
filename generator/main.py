#!/usr/bin/env python3
import yaml
from jinja2 import Template, Environment, FileSystemLoader

def main():
    print("MMW Generator")
    with open("../idls/HelloMessage.yml", "r") as file:
        messages_dict = yaml.safe_load(file)

        for message_name, message_fields_dict in messages_dict.items():

            print(messages_dict.items())
        
            # Load the folder containing your template files
            file_loader = FileSystemLoader("templates")
            env = Environment(loader=file_loader)

            # Select the specific file to execute
            template = env.get_template("message_template.h")

            # Pass data into the file and run it
            output = template.render(message_name=message_name, message_fields_dict=message_fields_dict)

            print(output)
            
            # For each message name in dictionary of messages, make the output .h files
            # for message_name in message_dict.keys():
            #     with open("output/" + message_name + ".h", "w", encoding="utf-8") as f:
            #         f.write(output)

if __name__=="__main__":
    main()
