# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/mongoose/include $(PROJECT_PATH)/components/mongoose/mongoose
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/mongoose -lmongoose
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += $(PROJECT_PATH)/components/mongoose/mongoose
COMPONENT_LIBRARIES += mongoose
COMPONENT_LDFRAGMENTS += 
component-mongoose-build: 
