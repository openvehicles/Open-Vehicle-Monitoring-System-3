#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
ifdef CONFIG_OVMS_COMP_PLUGINS
COMPONENT_SRCDIRS := src
COMPONENT_ADD_INCLUDEDIRS := src
COMPONENT_ADD_LDFLAGS = -Wl,--whole-archive -l$(COMPONENT_NAME) -Wl,--no-whole-archive
endif
endif
