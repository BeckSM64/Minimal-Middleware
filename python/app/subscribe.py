#!/usr/bin/python3
import mmw
import time
import signal
import sys

def my_callback(topic: str, msg: str):
    print(f"[Python Callback] Received: {msg}")

def my_callback_2(topic: str, msg: str):
    print(f"[Python Callback] Received: {msg}")

# Set the log level
# mmw.set_log_level(mmw.MmwLogLevel.MMW_LOG_LEVEL_OFF)

# Initialize the library
mmw.initialize("127.0.0.1", 5000)

# Create a subscriber object
subscriber = mmw.create_subscriber("Test Topic", my_callback)
subscriber_2 = mmw.create_subscriber("Test Topic 2", my_callback_2)

print("Waiting for messages... Ctrl+C to exit")

# Keep the main thread alive
while True:
    time.sleep(1)
