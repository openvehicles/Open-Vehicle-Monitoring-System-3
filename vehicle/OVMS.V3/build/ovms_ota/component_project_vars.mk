# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/ovms_ota/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/ovms_ota -Wl,--whole-archive -lovms_ota -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += ovms_ota
COMPONENT_LDFRAGMENTS += 
component-ovms_ota-build: 
