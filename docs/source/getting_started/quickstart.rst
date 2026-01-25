Quickstart
==========

This guide will get you publishing and subscribing with MMW in both C++ and Python.

Initialize
----------

Before sending messages, you must initialize the MMW runtime.

**C++**
::

    #include "MMW.h"

    if (mmw_initialize("127.0.0.1", 5000) != MMW_OK) {
        // handle error
    }

**Python**
::

    import mmw

    mmw.initialize("127.0.0.1", 5000)

Create Publisher
----------------

Publish messages to a topic.

**C++**
::

    mmw_create_publisher("example_topic");

**Python**
::

    mmw.create_publisher("example_topic")

Publish Message
---------------

**C++**
::

    mmw_publish("example_topic", "Hello World", MMW_BEST_EFFORT);

**Python**
::

    mmw.publish("example_topic", "Hello World", mmw.MmwReliability.MMW_BEST_EFFORT)

Subscribe to Topic
------------------

Receive messages from a topic.

**C++**
::

    void callback(const char* topic, const char* message) {
        // handle message
    }

    mmw_create_subscriber("example_topic", callback);

**Python**
::

    def callback(topic, message):
        # handle message

    sub = mmw.create_subscriber("example_topic", callback)

Cleanup
-------

Always clean up before exiting.

**C++**
::

    mmw_cleanup();

**Python**
::

    mmw.cleanup()
