# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/crypto
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/crypto -Wl,--whole-archive -lcrypto -Wl,--no-whole-archive
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += crypto
COMPONENT_LDFRAGMENTS += 
component-crypto-build: 
