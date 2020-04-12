# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/can/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/can -Wl,--whole-archive -lcan -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += can
COMPONENT_LDFRAGMENTS += 
component-can-build: 
