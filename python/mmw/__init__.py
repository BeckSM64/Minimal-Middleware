# import os
# from pathlib import Path

# # Add MinGW bin dir first
# os.add_dll_directory(r"C:\msys64\mingw64\bin")

# # Add package folder for libmmw.dll
# os.add_dll_directory(str(Path(__file__).parent))

# from .mmw import *

import os
from pathlib import Path
import sys

# Only needed on Windows for DLL search paths
if sys.platform.startswith("win"):
    # Add MinGW runtime DLLs
    os.add_dll_directory(r"C:\msys64\mingw64\bin")
    # Add the folder containing libmmw.dll
    os.add_dll_directory(str(Path(__file__).parent))

from .mmw import *

