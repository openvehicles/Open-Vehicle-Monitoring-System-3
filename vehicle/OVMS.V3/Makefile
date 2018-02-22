#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := ovms3
OVMS_VERSION := $(shell git describe --always --tags --dirty)
CPPFLAGS := -D OVMS_VERSION=\"$(OVMS_VERSION)\" $(CPPFLAGS)

include $(IDF_PATH)/make/project.mk
