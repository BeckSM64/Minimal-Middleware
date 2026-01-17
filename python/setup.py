import os
import shutil
from setuptools import setup, Extension
import pybind11

# Resolve absolute paths
BASE_DIR = os.path.abspath(os.path.dirname(__file__))
ROOT_DIR = os.path.dirname(BASE_DIR)
BUILD_DIR = os.path.join(ROOT_DIR, "build")
INCLUDE_DIR = os.path.join(ROOT_DIR, "includes")

# Copy the library into the current directory so it can be bundled/linked
shutil.copy(os.path.join(BUILD_DIR, "libmmw.so"), BASE_DIR)

ext = Extension(
    "mmw_python",
    sources=["bindings.cpp"],
    include_dirs=[pybind11.get_include(), INCLUDE_DIR],
    library_dirs=[BASE_DIR],
    libraries=["mmw"],
    # $ORIGIN: look for libmmw.so in the same folder as this module
    runtime_library_dirs=["$ORIGIN"],
    extra_link_args=["-Wl,--disable-new-dtags"],
    language='c++',
)

setup(
    name="mmw_python",
    version="0.1.0",
    ext_modules=[ext],
    # Bundle the .so inside the wheel/package
    package_data={'': ['libmmw.so']},
    include_package_data=True,
    zip_safe=False,
)
