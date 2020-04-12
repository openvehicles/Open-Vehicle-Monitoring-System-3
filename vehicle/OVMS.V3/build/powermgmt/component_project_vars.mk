# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/powermgmt
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/powermgmt -Wl,--whole-archive -lpowermgmt -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += powermgmt
COMPONENT_LDFRAGMENTS += 
component-powermgmt-build: 
