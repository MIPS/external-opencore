# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvrtspsmplugininterface

XINCDIRS += ../../../../../../../oscl/oscl/oscllib/src ../../../../../../../nodes/streaming/streamingmanager/plugins/rtspunicast/include
SRCDIR := ../../src

SRCS := pvmf_smnode_rtspunicastmanager_plugin_interface.cpp

include $(MK)/library.mk

