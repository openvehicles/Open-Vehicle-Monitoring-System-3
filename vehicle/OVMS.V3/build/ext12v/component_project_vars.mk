# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/ext12v/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/ext12v -Wl,--whole-archive -lext12v -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += ext12v
COMPONENT_LDFRAGMENTS += 
component-ext12v-build: 
