# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/pcp
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/pcp -Wl,--whole-archive -lpcp -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += pcp
COMPONENT_LDFRAGMENTS += 
component-pcp-build: 
