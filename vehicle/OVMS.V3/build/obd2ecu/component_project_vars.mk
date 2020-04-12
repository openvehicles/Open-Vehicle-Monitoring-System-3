# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/obd2ecu/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/obd2ecu -Wl,--whole-archive -lobd2ecu -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += obd2ecu
COMPONENT_LDFRAGMENTS += 
component-obd2ecu-build: 
