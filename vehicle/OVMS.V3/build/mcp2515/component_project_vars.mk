# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/mcp2515/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/mcp2515 -Wl,--whole-archive -lmcp2515 -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += mcp2515
COMPONENT_LDFRAGMENTS += 
component-mcp2515-build: 
