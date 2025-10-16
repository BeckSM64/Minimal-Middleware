# Minimal Middleware (MMW)

MMW is a lightweight, broker-based publish/subscribe middleware implemented in modern C++.
It allows applications to easily exchange messages across processes or systems using simple network sockets and JSON serialization.

# ✨ Features

- C++11+ compatible
- Header-only serialization using nlohmann::json
- Cross-platform TCP communication
- spdlog-based logging
- Simple interface for publishers and subscribers
- Extensible message format (supports future typed messages)

# 📦 Building
## Requirements

- CMake 3.15+
- A C++11-compatible compiler
- Internet access (for FetchContent dependencies)

## Steps
```bash
git clone https://github.com/BeckSM64/Minimal-Middleware.git
cd Minimal-Middleware
mkdir -p build/
cd build/
cmake ../
make
```


## This Builds

- broker
    - The central message router
- Example applications
    - publish
    - subscribe
    - publish_c
    - subscribe_c

# ⚙️ Configuration

config.yml controls the broker connection:

```yaml
    broker:
        - brokerIp: 127.0.0.1
        - brokerPort: 5000
```

Applications will read this in at runtime.

# 🚀 Usage
1. Start the Broker
    - ```./broker```
    - The broker listens for publisher and subscriber connections and routes messages by topic.

2. Start a Subscriber
    - ```./subscribe```


### Example subscriber output:

```bash
Successfully parsed config file
Broker IP: 127.0.0.1
Broker Port: 5000
Subscriber connected to broker
Received: Hello, World!
```

3. Start a Publisher
    - ```./publish```


### Example publisher output:

```bash
Successfully parsed config file
Publisher connected to broker
Publishing message on topic 'example_topic'
```

# 🧩 API Overview

## Include the interface

```c++
#include "MMW.h"
```

## Publisher

```c++
mmw_initialize("config.yml");
mmw_create_publisher("example_topic");
mmw_publish("example_topic", "Hello, world!");
mmw_cleanup();
```

## Subscriber

```c++
void some_user_defined_callback(const char* message) {
    spdlog::info("Received: {}", message);
}

mmw_initialize("config.yml");
mmw_create_subscriber("example_topic", some_user_defined_callback);
```

# 🔒 Return Codes

## All interface functions return an MmwResult enum

```c++
typedef enum {
    MMW_OK,
    MMW_ERROR
} MmwResult;
```

# 🧠 Future Work

- Typed message publishing
- Zero-copy serialization options
- Encrypted or compressed transport
- Broker clustering
