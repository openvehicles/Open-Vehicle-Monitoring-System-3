#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

ifdef CONFIG_OVMS_SC_GPL_WOLF
COMPONENT_ADD_INCLUDEDIRS := wolfssh
COMPONENT_EXTRA_INCLUDES := ${IDF_PATH}/components/freertos/include/freertos
COMPONENT_OBJS := wolfssh/src/internal.o wolfssh/src/io.o wolfssh/src/keygen.o wolfssh/src/log.o wolfssh/src/port.o wolfssh/src/ssh.o
COMPONENT_SRCDIRS := wolfssh/src
CFLAGS += -DWOLFSSL_USER_SETTINGS
CFLAGS += -Wno-maybe-uninitialized
#CFLAGS += -DDEBUG_WOLFSSH
endif
