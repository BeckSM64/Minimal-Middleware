#!/usr/bin/python3
import mmw_python as mmw
import time
import signal
import sys

def my_callback(msg: str):
    print(f"[Python Callback] Received: {msg}")

# Set the log level
mmw.set_log_level(mmw.MmwLogLevel.MMW_LOG_LEVEL_OFF)

# Initialize the library
mmw.initialize("127.0.0.1", 5000)

# Create a subscriber object
subscriber = mmw.Subscriber("Test Topic", my_callback)

print("Waiting for messages... Ctrl+C to exit")

# Keep the main thread alive
while True:
    time.sleep(1)
