# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/pushover/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/pushover -Wl,--whole-archive -lpushover -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += pushover
COMPONENT_LDFRAGMENTS += 
component-pushover-build: 
