# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/vehicle
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/vehicle -Wl,--whole-archive -lvehicle -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += vehicle
COMPONENT_LDFRAGMENTS += 
component-vehicle-build: 
