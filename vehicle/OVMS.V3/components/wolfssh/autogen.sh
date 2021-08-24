#!/bin/sh
#
# Create configure and makefile stuff...
#

set -e

# if get an error about libtool not setup
# " error: Libtool library used but 'LIBTOOL' is undefined
#     The usual way to define 'LIBTOOL' is to add 'LT_INIT' "
# manually call libtoolize or glibtoolize before running this again
# (g)libtoolize

# if you get an error about config.rpath missing, some buggy automake versions
# then touch the missing file (may need to make config/ first).
# touch config/config.rpath
# touch config.rpath

if test ! -d build-aux; then
  echo "Making missing build-aux directory."
  mkdir -p build-aux
fi

if test ! -f build-aux/config.rpath; then
  echo "Touching missing build-aux/config.rpath file."
  touch build-aux/config.rpath
fi

# Git hooks should come before autoreconf.
if test -d .git; then
  if ! test -d .git/hooks; then
    mkdir .git/hooks
  fi
  ln -s -f ../../scripts/pre-commit.sh .git/hooks/pre-commit
fi

# If this is a source checkout then call autoreconf with error as well
if test -e .git; then
  WARNINGS="all,error"
else
  WARNINGS="all"
fi

autoreconf --install --force --verbose 
