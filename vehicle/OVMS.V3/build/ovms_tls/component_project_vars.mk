# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/ovms_tls/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/ovms_tls -Wl,--whole-archive -lovms_tls -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += ovms_tls
COMPONENT_LDFRAGMENTS += 
component-ovms_tls-build: 
