# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(IDF_PATH)/components/newlib/platform_include $(IDF_PATH)/components/newlib/include
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/newlib $(IDF_PATH)/components/newlib/lib/libc-psram-workaround.a $(IDF_PATH)/components/newlib/lib/libm-psram-workaround.a -lnewlib
COMPONENT_LINKER_DEPS += $(IDF_PATH)/components/newlib/lib/libc-psram-workaround.a $(IDF_PATH)/components/newlib/lib/libm-psram-workaround.a
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += newlib
COMPONENT_LDFRAGMENTS += $(IDF_PATH)/components/newlib/esp32-spiram-rom-functions.lf
component-newlib-build: 
