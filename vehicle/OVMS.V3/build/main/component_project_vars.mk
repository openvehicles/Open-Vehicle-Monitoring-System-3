# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/main
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/main -Wl,--whole-archive -lmain -Wl,--no-whole-archive -T main/ovms_boot.ld -mfix-esp32-psram-cache-issue
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += main
COMPONENT_LDFRAGMENTS += 
component-main-build: 
