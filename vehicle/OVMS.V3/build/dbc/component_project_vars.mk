# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/dbc/src $(PROJECT_PATH)/components/dbc/yacclex
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/dbc -Wl,--whole-archive -ldbc -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += dbc
COMPONENT_LDFRAGMENTS += 
component-dbc-build: 
