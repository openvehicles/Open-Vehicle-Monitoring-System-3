#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

ifdef CONFIG_OVMS_SC_GPL_WOLF
COMPONENT_ADD_INCLUDEDIRS := .
COMPONENT_EXTRA_INCLUDES := ${IDF_PATH}/components/freertos/include/freertos
COMPONENT_OBJS := src/internal.o src/io.o src/keygen.o src/log.o src/port.o src/ssh.o
COMPONENT_SRCDIRS := src
CFLAGS += -DWOLFSSL_USER_SETTINGS
CFLAGS += -Wno-maybe-uninitialized
#CFLAGS += -DDEBUG_WOLFSSH
endif
