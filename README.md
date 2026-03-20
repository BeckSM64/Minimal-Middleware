# Minimal Middleware (MMW)

MMW is a lightweight, broker-based publish/subscribe middleware implemented in C++.
It allows applications to easily exchange messages across processes or systems using simple network sockets and JSON or binary serialization.

# ✨ Features

- C++11+ compatible
- C-compatible interface
- Configurable serialization using nlohmann::json or cereal
- Cross-platform TCP communication (Linux/Windows)
- spdlog-based logging
- Simple interface for publishers and subscribers
- Extensible message format

# 📝 Documentation

API documentation is handled with Doxygen and Sphinx. It will eventually be located in a more permanent place, but for now you can generate the documentation by running the following task command
```bash
task docs
```
This will build the documentation in the docs/build/html/ directory. You can then view it by opening the index.html in a web browser of your choice.

# 📦 Building
## Requirements

- CMake 3.15+
- A C++11-compatible compiler
- Internet access (for FetchContent dependencies)

## Dependencies
- [spdlog](https://github.com/gabime/spdlog) — Fast C++ logging library
- [nlohmann/json](https://github.com/nlohmann/json) — JSON serialization for modern C++
- [cereal](https://github.com/USCiLab/cereal) — Header-only C++11 serialization library
- [pybind11](https://github.com/pybind/pybind11) — Seamless C++/Python bindings
- [SQLite3](https://www.sqlite.org/index.html) — Lightweight relational database for persistence
- [task](https://taskfile.dev/) — A cross platform build tool inspired by Make

Dependencies are fetched at build time via CMake FetchContent. Please refer to each library's repository for license information.

## Steps
```bash
git clone https://github.com/BeckSM64/Minimal-Middleware.git
cd Minimal-Middleware
mkdir -p build/
cd build/
cmake ../ -DBUILD_BROKER=ON -DBUILD_SAMPLE_APPS=ON -DCEREAL_SERIALIZER=ON
make
```
or you can simply run the Taskfile commands
```bash
task --list-all
```
This will list all the build options.


## This Builds

- broker
    - The central message router
- Example applications
    - publish
    - subscribe
    - publish_c
    - subscribe_c
    - publish_raw
    - subscribe_raw

## Fetch Content
You can also easily integrate the library into your project if you're using CMake
```bash
include(FetchContent)
FetchContent_Declare(
  mmw
  GIT_REPOSITORY https://github.com/BeckSM64/Minimal-Middleware.git
  GIT_TAG main # can be a specific tag or branch
)
FetchContent_MakeAvailable(mmw)
```

# 🚀 Usage
1. Start the Broker
    - ```./broker 5000```
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
mmw_set_log_level(MMW_LOG_LEVEL_INFO);
mmw_initialize("127.0.0.1", 5000); // The IP and port for the broker
mmw_create_publisher("example_topic");
mmw_publish("example_topic", "Hello, world!", MMW_RELIABLE); // can also be MMW_BEST_EFFORT
mmw_cleanup();
```

## Subscriber

```c++
void some_user_defined_callback(const char* message) {
    spdlog::info("Received: {}", message);
}

mmw_set_log_level(MMW_LOG_LEVEL_INFO);
mmw_initialize("127.0.0.1", 5000);
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

# 📝 License
This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.
