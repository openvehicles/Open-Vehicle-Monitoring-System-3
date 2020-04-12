# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/retools/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/retools -Wl,--whole-archive -lretools -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += retools
COMPONENT_LDFRAGMENTS += 
component-retools-build: 
