# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/zip/include $(PROJECT_PATH)/components/zip/zlib $(PROJECT_PATH)/components/zip/libzip/lib
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/zip -Wl,--whole-archive -lzip -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += zip
COMPONENT_LDFRAGMENTS += 
component-zip-build: 
