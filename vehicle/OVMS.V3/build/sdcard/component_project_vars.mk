# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/sdcard/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/sdcard -Wl,--whole-archive -lsdcard -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += sdcard
COMPONENT_LDFRAGMENTS += 
component-sdcard-build: 
