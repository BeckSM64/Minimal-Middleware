#!/usr/bin/python3
import mmw_python
import time

def my_callback(msg: str):
    print(f"[Python Callback] Received: {msg}")

mmw_python.initialize("config.yml")
mmw_python.create_subscriber("Test Topic", my_callback)

print("Waiting for messages... Ctrl+C to exit")
counter = 100
try:
    while counter > 0:
        time.sleep(0.1)
        counter -= 1
except KeyboardInterrupt:
    print("Exiting...")
finally:
    mmw_python.cleanup()

mmw_python.cleanup()