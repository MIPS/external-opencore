# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvrtspsmpluginreginterface

SRCDIR := ../../src
XINCDIRS += ../../../../../../../oscl/oscl/oscllib/src ../../../../../../../nodes/streaming/streamingmanager/plugins/rtspunicast/include
XINCDIRS += ../../../../../../../nodes/streaming/streamingmanager/plugins/common/include

SRCS := pvmf_smnode_rtspunicast_plugin_registry_populator_interface.cpp

include $(MK)/library.mk

