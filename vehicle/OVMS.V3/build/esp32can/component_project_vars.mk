# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/esp32can/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/esp32can -Wl,--whole-archive -lesp32can -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += esp32can
COMPONENT_LDFRAGMENTS += 
component-esp32can-build: 
