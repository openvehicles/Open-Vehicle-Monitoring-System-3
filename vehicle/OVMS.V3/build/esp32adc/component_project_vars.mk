# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/esp32adc/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/esp32adc -Wl,--whole-archive -lesp32adc -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += esp32adc
COMPONENT_LDFRAGMENTS += 
component-esp32adc-build: 
