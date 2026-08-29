API Documentation
==================

MMW provides a number of functions via the C-Compatible API to assist in publishing and subscribing messages

Logging
-------

**C API**

.. code-block:: c

    void mmw_set_log_level(MmwLogLevel level);

**Python**

.. code-block:: python

    mmw.set_log_level(level)

This function allows the user to choose from various log levels, for debug purposes, within their application. The following log levels are supported via the ``MmwLogLevel`` enum

.. code-block:: c

    typedef enum {
        MMW_LOG_LEVEL_OFF,
        MMW_LOG_LEVEL_ERROR,
        MMW_LOG_LEVEL_WARN,
        MMW_LOG_LEVEL_INFO,
        MMW_LOG_LEVEL_DEBUG,
        MMW_LOG_LEVEL_TRACE
    } MmwLogLevel;

Initialization
--------------
**C API**

.. code-block:: c

    MmwResult mmw_initialize(const char* brokerIp, unsigned short port);

**Python**

.. code-block:: python

    mmw.initialize(broker_ip, port)

This function is responsible for setting the IP address and port of the broker, as well as setting up the global serializer. It tells the MMW library that all publishers and subscribers that are created will connect to the broker at the specified IP and port. ``mmw_initialize`` must be called before any publishers or subscribers are created. Returns a ``MmwResult``.

Publishers
----------
**C API**

.. code-block:: c

    MmwResult mmw_create_publisher(const char* topic);

**Python**

.. code-block:: python

    mmw.create_publisher(topic)

Publishers can be created by calling this create publisher call. It takes a topic as an argument. This will create a dedicated TCP connection to the broker. It must be called before any messages can be published on a topic. Returns a ``MmwResult``.

**C API**

.. code-block:: c

    MmwResult mmw_publish(const char* topic, const char* message, MmwReliability reliability);

**Python**

.. code-block:: python

    mmw.publish(topic, message, reliability)

Publishes a message as a string over a specified topic. Messages can be published as either reliable or best effort. This is configured via the ``MmwReliability`` enum. Returns a ``MmwResult``.

**C API**

.. code-block:: c

    MmwResult mmw_publish_raw(const char* topic, void* message, size_t size, MmwReliability reliability);

**Python**

.. code-block:: python

    # Not currently supported for Python

Publishes a message as raw bytes over a specified topic. Messages should be structured as basic C structs containing network safe types. Must also provide the size of the structure being sent, as well as the desired reliability of the message via the ``MmwReliability`` enum. Returns a ``MmwResult``.

**C API**

.. code-block:: c

    MmwResult mmw_delete_publisher(const char* topic);

**Python**

.. code-block:: python

    mmw.delete_publisher(topic)

Deletes a publihser. Takes a topic as an argument to delete the associated publisher. Returns a ``MmwResult``.

Subscribers
-----------
**C API**

.. code-block:: c

    MmwResult mmw_create_subscriber(const char* topic, void (*mmw_callback)(const char*, const char*));

**Python**

.. code-block:: python

    mmw.create_subscriber(topic, callback)

Creates a subscriber that receives string messages. Takes a topic and a callback as arguments. The callback is a simple C function which takes a string topic and a string message as arguments. Upon receiving an incoming string message from a publisher over the specified topic, the callback will be fired and execute whatever code it contains. The ``mmw_create_subscriber`` call returns a ``MmwResult``.

.. code-block:: c

    MmwResult mmw_create_subscriber_raw(const char* topic, void (*mmw_callback)(const char*, void*));

.. code-block:: python

    # Not currently supported for Python

Creates a subscriber that receives raw byte messages. Takes a topic and a callback as arguments. The callback is a simple C function which takes a string topic and a void pointer message as arguments. Upon receiving an incoming raw byte message from a publisher over the specified topic, the callback will be fired and execute whatever code it contains. The ``mmw_create_subscriber_raw`` call returns a ``MmwResult``.

**C API**

.. code-block:: c

    MmwResult mmw_delete_subscriber(const char* topic);

**Python**

.. code-block:: python

    mmw.delete_subscriber(topic)

Deletes a subscriber. Takes a topic as an argument to delete the associated subscriber. Returns a ``MmwResult``.

Global
------

**C API**

.. code-block:: c

    MmwResult mmw_cleanup();

**Python**

.. code-block:: python

    mmw.cleanup()

Deletes all existing publishers and subscribers. Returns a ``MmwResult``.