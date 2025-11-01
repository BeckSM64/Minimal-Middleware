#!/usr/bin/python3
import mmw_python
import time
import signal
import sys

def my_callback(msg: str):
    print(f"[Python Callback] Received: {msg}")

# Initialize the library
mmw_python.initialize("127.0.0.1", 5000)

# Create a subscriber object
subscriber = mmw_python.Subscriber("Test Topic", my_callback)

print("Waiting for messages... Ctrl+C to exit")

# Keep the main thread alive
while True:
    time.sleep(1)
