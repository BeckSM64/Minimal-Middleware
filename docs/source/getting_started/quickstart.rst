Quickstart
==========

This guide shows the minimal steps required to publish and subscribe to messages
using MMW from both C++ and Python.

MMW follows a simple lifecycle:

1. Initialize the runtime
2. Create publishers and subscribers
3. Send and receive messages
4. Clean up on shutdown

Initialize
----------

The MMW runtime must be initialized before any publishers or subscribers
are created.

**C++**

::

    #include "MMW.h"

    if (mmw_initialize("127.0.0.1", 5000) != MMW_OK) {
        // initialization failed
    }

**Python**

::

    import mmw

    mmw.initialize("127.0.0.1", 5000)

Create a Publisher
------------------

Publishers are associated with a topic. Creating a publisher does not send
any messages by itself.

**C++**

::

    mmw_create_publisher("example_topic");

**Python**

::

    mmw.create_publisher("example_topic")

Publish a Message
-----------------

Messages are sent to a topic with an explicit reliability mode.

**C++**

::

    mmw_publish(
        "example_topic",
        "Hello World",
        MMW_BEST_EFFORT
    );

**Python**

::

    mmw.publish(
        "example_topic",
        "Hello World",
        mmw.MmwReliability.MMW_BEST_EFFORT
    )

Create a Subscriber
-------------------

Subscribers register a callback that is invoked when a message is received.

**C++**

::

    void on_message(const char* topic, const char* message)
    {
        // handle message
    }

    mmw_create_subscriber("example_topic", on_message);

**Python**

::

    def on_message(topic, message):
        # handle message
        pass

    mmw.create_subscriber("example_topic", on_message)

Shutdown
--------

All resources must be released before program exit.

**C++**

::

    mmw_cleanup();

**Python**

::

    mmw.cleanup()
