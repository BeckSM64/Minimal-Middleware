# ✅ Minimum ready-for-release checklist

1. Core functionality works
    - Publishers/subscribers connect correctly ✅
    - Messages and raw messages send/receive reliably ✅
    - ACK/retries work for reliable messages ✅
2. Error handling
    - All socket/serialization calls either log or return MMW_ERROR
    - Threads and dynamic allocations checked
3. Configurable settings
    - Broker port is configurable (not hardcoded)
    - Optional: broker IP configurable from client
4. Clean shutdown
    - mmw_cleanup() closes sockets, stops threads, deletes flags, cleans serializer
5. Documentation & examples
    - Basic README showing how to create publisher/subscriber, send messages
    - Example code is enough for people to test immediately
6. License & public repo
    - MIT license added
    - Repo ready to be public

# Optional but strongly recommended for v1
- Logging levels configurable (so spdlog messages don’t always flood stdout)
- Some basic unit tests to ensure core functions work on Linux/Windows
- Maybe a minimal “Hello world” example program