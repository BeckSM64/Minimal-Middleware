Publish / Subscribe
==================

MMW uses a broker-based publish/subscribe model. Topics are strings that both publishers and subscribers reference.

Publisher
---------

A publisher sends messages to a topic. Multiple publishers can send to the same topic. Messages can be:

- **Reliable:** guaranteed to be delivered
- **Best Effort:** may be dropped under load

Subscriber
----------

A subscriber receives messages from one or more topics. Each subscriber must register a callback function that handles incoming messages.

Example (Python)
::

    import mmw

    def on_message(topic, message):
        print(f"Received on {topic}: {message}")

    sub = mmw.create_subscriber("example_topic", on_message)

Message Flow
------------

1. Publisher sends message to a topic.
2. Broker routes the message to all subscribers of that topic.
3. Subscriber callback is invoked asynchronously.

Notes
-----

- Messages are delivered asynchronously. Do not block in your callback.
- Subscribers can exist in multiple processes or threads.
- The broker ensures reliable delivery for messages published as `RELIABLE`.
