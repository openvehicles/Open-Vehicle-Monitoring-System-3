# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/vehicle_demo/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/vehicle_demo -Wl,--whole-archive -lvehicle_demo -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += vehicle_demo
COMPONENT_LDFRAGMENTS += 
component-vehicle_demo-build: 
