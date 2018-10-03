#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

ifdef CONFIG_OVMS_SC_ZIP
COMPONENT_ADD_INCLUDEDIRS := include zlib libzip/lib
COMPONENT_SRCDIRS := zlib libzip/lib src
COMPONENT_OBJS := 
include $(COMPONENT_PATH)/component_objs.mk
COMPONENT_OBJS += src/zip_archive.o
COMPONENT_SUBMODULES := 
CFLAGS += -Wno-pointer-sign -Wno-implicit-function-declaration -Wno-maybe-uninitialized -Wno-unused-but-set-variable
CFLAGS += -DHAVE_CONFIG_H
CFLAGS += -Wstack-usage=512
CXXFLAGS += -Wstack-usage=512
COMPONENT_ADD_LDFLAGS = -Wl,--whole-archive -l$(COMPONENT_NAME) -Wl,--no-whole-archive
endif
