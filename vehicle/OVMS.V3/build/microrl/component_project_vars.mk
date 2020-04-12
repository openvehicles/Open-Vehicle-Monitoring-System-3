# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/microrl
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/microrl -Wl,--whole-archive -lmicrorl -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += microrl
COMPONENT_LDFRAGMENTS += 
component-microrl-build: 
