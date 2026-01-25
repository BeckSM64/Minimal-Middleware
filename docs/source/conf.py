# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'MMW'
copyright = '2026, Shane Harrington'
author = 'Shane Harrington'
release = 'v2.0.0'

# Add these extensions
# extensions = [
#     "sphinx.ext.autodoc",  # Python API
#     "sphinx.ext.napoleon", # Google / NumPy style docstrings
#     "breathe",             # C++ API via Doxygen
#     "exhale"               # auto-generate C++ API pages
# ]

# HTML theme
html_theme = "sphinx_rtd_theme"

# Breathe config (Doxygen XML location)
breathe_projects = { "MMW": "../doxygen/xml" }
breathe_default_project = "MMW"

# Exhale config (C++ API layout)
# exhale_args = {
#     "containmentFolder": "./api/cpp",
#     "rootFileName": "index.rst",
#     "rootFileTitle": "C++ API Reference",
#     "doxygenStripFromPath": "..",
#     "createTreeView": True,
# }
