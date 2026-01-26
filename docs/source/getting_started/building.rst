Building MMW
============

Requirements
------------

* C++11 compiler
* CMake >= 3.0
* Python >= 3.11 (optional, for Python bindings)
* Supported OS: Linux, Windows

Building the Core Library
-------------------------

.. code-block:: bash

   git clone https://github.com/BeckSM64/Minimal-Middleware.git
   cd Minimal-Middleware
   mkdir build && cd build
   cmake .. -DBUILD_BROKER=ON -DBUILD_SAMPLE_APPS=ON -DBUILD_PYTHON_MODULE=ON
   make

On Windows, MinGW is supported. When using MinGW, invoke CMake with:

.. code-block:: bash

   -G "Unix Makefiles"

Building the Python Bindings
----------------------------

The Python bindings are optional and require Python 3.11 or newer.

.. code-block:: bash

   cd Minimal-Middleware/python
   python3 -m pip install -r requirements.txt
   python3 -m build
   python3 -m pip install dist/*.whl

Static vs Shared Builds
-----------------------

MMW builds as a shared library by default.

When building the Python bindings on Windows, the library is forced to build
statically to avoid DLL resolution issues inside the generated wheel.

To explicitly disable shared library builds, pass the following option to CMake:

.. code-block:: bash

   -DBUILD_SHARED_LIBRARY=OFF

Troubleshooting
---------------

Windows Toolchain Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On Windows, CMake must be told where the vcpkg toolchain file is located so that
system dependencies (such as SQLite) can be found.

Example:

.. code-block:: bash

   -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake

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
