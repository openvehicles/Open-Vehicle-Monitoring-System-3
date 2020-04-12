# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/esp32system
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/esp32system -Wl,--whole-archive -lesp32system -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += esp32system
COMPONENT_LDFRAGMENTS += 
component-esp32system-build: 
