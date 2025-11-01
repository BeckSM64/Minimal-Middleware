#!/usr/bin/python3
import mmw_python as mmw

mmw.initialize("127.0.0.1", 5000)
mmw.create_publisher("Test Topic")
mmw.publish("Test Topic", "hello from python!")

