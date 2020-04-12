# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/ovms_webserver/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/ovms_webserver -Wl,--whole-archive -lovms_webserver -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += ovms_webserver
COMPONENT_LDFRAGMENTS += 
component-ovms_webserver-build: 
