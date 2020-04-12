# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/vehicle_obdii/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/vehicle_obdii -Wl,--whole-archive -lvehicle_obdii -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += vehicle_obdii
COMPONENT_LDFRAGMENTS += 
component-vehicle_obdii-build: 
