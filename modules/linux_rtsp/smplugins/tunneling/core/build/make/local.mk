# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvsmrtsptstreamingplugin

XINCDIRS += ../../../../../../../oscl/oscl/oscllib/src ../../../../../../../nodes/streaming/streamingmanager/plugins/rtsptunicast/include

SRCDIR := ../../src

SRCS := pvmf_smnode_rtsptunicastmanager_plugin_interface.cpp

include $(MK)/library.mk

