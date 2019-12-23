#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
COMPONENT_ADD_INCLUDEDIRS:=src
COMPONENT_SRCDIRS:=src
CFLAGS += -DDUK_USE_GET_MONOTONIC_TIME_CLOCK_GETTIME
#COMPONENT_ADD_LDFLAGS = -Wl,--whole-archive -l$(COMPONENT_NAME) -Wl,--no-whole-archive
endif
