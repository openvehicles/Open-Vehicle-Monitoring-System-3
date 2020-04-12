# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/canopen/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/canopen -Wl,--whole-archive -lcanopen -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += canopen
COMPONENT_LDFRAGMENTS += 
component-canopen-build: 
