# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/ovms_server/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/ovms_server -Wl,--whole-archive -lovms_server -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += ovms_server
COMPONENT_LDFRAGMENTS += 
component-ovms_server-build: 
