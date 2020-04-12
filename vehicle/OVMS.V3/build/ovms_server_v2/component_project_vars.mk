# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/ovms_server_v2/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/ovms_server_v2 -Wl,--whole-archive -lovms_server_v2 -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += ovms_server_v2
COMPONENT_LDFRAGMENTS += 
component-ovms_server_v2-build: 
