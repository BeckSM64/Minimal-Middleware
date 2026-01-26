Building MMW
============

Requirements
------------

* C++11 compiler
* CMake >= 3.0
* Python >= 3.11 (optional, for bindings)
* OS: Linux / Windows

Building the Core Library
-------------------------

.. code-block:: bash

   git clone https://github.com/BeckSM64/Minimal-Middleware.git
   cd Minimal-Middleware/
   mkdir -p build/ && cd build/
   cmake ../ -DBUILD_BROKER=ON -DBUILD_SAMPLE_APPS=ON -DBUILD_PYTHON_MODULE=ON
   make

Building the Python Bindings
----------------------------

.. code-block:: bash

   cd Minimal-Middleware/python/
   python3 -m pip install -r requirements.txt
   python3 -m build
   python3 -m pip install dist/mmw-cython.0.1.0.whl

Static vs Shared Builds
-----------------------

(brief explanation)

Troubleshooting
---------------

(common errors you already hit)
