# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/spinodma
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/spinodma -Wl,--whole-archive -lspinodma -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += spinodma
COMPONENT_LDFRAGMENTS += 
component-spinodma-build: 
