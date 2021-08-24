# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# http://www.sphinx-doc.org/en/master/config

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
# import os
# import sys
# sys.path.insert(0, os.path.abspath('.'))


# -- Project information -----------------------------------------------------

project = 'Open Vehicles'
copyright = '2019-2020, Open Vehicles Developers'
author = 'Open Vehicle Developers'


# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
  'm2r',
]

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = [
  'docs',
  '*/**.md',
  '*/wolfssl/*',
  '*/duktape/*',
  '*/mongoose/*',
]

master_doc = 'index'


# -- Source configuration ----------------------------------------------------

# - |clearfix| global substitution to clear floats (e.g. images)
rst_epilog = """
.. |clearfix| raw:: html

  <div class="clearfix"></div>

"""


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
#html_theme = 'default'
html_theme = 'sphinx_rtd_theme'

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']

html_context = {
}

def setup(app):
    app.add_css_file('theme_overrides.css')
    app.add_css_file('copybutton.css')
    app.add_js_file('clipboard.min.js')
    app.add_js_file('copybutton.js')

