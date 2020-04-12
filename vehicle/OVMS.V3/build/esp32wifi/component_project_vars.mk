# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/esp32wifi/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/esp32wifi -Wl,--whole-archive -lesp32wifi -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += esp32wifi
COMPONENT_LDFRAGMENTS += 
component-esp32wifi-build: 
