Building MMW
============

Requirements
------------

* C++11 compiler
* CMake >= 3.5
* Python >= 3.11 (optional, for Python bindings)
* Supported OS: Linux, Windows, MacOS

Building the Core Library
-------------------------

The library can be built using task

.. code-block:: bash

   git clone https://github.com/BeckSM64/Minimal-Middleware.git
   cd Minimal-Middleware
   task build

This will build the core library. There are several variations to this build which can be made by passing various arguments to the task command

.. code-block:: bash

   task build BROKER=ON EXAMPLES=ON PYTHON=ON SERIALIZER=JSON_SERIALIZER

If you just want to build everything, you can run the following

.. code-block:: bash

   task build:all

Building the Python Bindings
----------------------------

The Python bindings are optional and require Python 3.11 or newer. They will be built when you run the build all command, or you can explicitly build them with the following

.. code-block:: bash

   task build PYTHON=ON

This will build and install the Python whl on any supported platform (ie. Windows, Linux, and MacOS)

Static vs Shared Builds
-----------------------

MMW builds as a static library by default.

To explicitly enable shared library builds, pass the following option to task:

.. code-block:: bash

   build SHARED=ON

Note that this may cause issues when building the whl for Python bindings. If you really want a shared library with the whl, you'll need to run the following on Linux and MacOS

.. code-block:: bash

   python3 -m pip auditwheel repair /path/to/whl.whl --add-path /path/to/shared/libraries/

and this on Windows

.. code-block:: bash

   python -m pip delvewheel repair /path/to/whl.whl --add-path /path/to/shared/libraries/

Troubleshooting
---------------

Windows Toolchain Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On Windows, CMake must be told where the vcpkg toolchain file is located so that
system dependencies (such as SQLite) can be found.

Example:

.. code-block:: bash

   task build VCPKG_PATH='C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake'

Dependencies
~~~~~~~~~~~~

MMW depends on the following libraries:

* spdlog — logging
* nlohmann/json — JSON serialization
* cereal — C++11 serialization (header-only)
* pybind11 — C++/Python bindings
* SQLite3 — broker persistence

SQLite3 is the only dependency requiring a system-level installation
(`apt install sqlite3-dev` on Linux or `vcpkg install sqlite3` on Windows).

All other dependencies are fetched automatically via CMake FetchContent.
