# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/console_ssh/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/console_ssh -Wl,--whole-archive -lconsole_ssh -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += console_ssh
COMPONENT_LDFRAGMENTS += 
component-console_ssh-build: 
