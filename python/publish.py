#!/usr/bin/python3
import mmw_python as mmw

mmw.initialize("config.yml")
mmw.create_publisher("Test Topic")
mmw.publish("Test Topic", "hello from python!")

