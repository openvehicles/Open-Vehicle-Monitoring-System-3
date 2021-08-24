#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
COMPONENT_ADD_INCLUDEDIRS := include mongoose
COMPONENT_PRIV_INCLUDEDIRS := ../wolfssl ../wolfssl/wolfssl
COMPONENT_EXTRA_INCLUDES := ${IDF_PATH}/components/freertos/include/freertos
COMPONENT_SRCDIRS := mongoose
COMPONENT_SUBMODULES := mongoose
#COMPONENT_ADD_LDFLAGS = -Wl,--whole-archive -l$(COMPONENT_NAME) -Wl,--no-whole-archive
endif
