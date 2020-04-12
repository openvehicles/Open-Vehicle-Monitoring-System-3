# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/vfsedit/src
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/vfsedit -Wl,--whole-archive -lvfsedit -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += vfsedit
COMPONENT_LDFRAGMENTS += 
component-vfsedit-build: 
