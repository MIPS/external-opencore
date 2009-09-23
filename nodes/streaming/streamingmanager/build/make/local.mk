# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvstreamingmanagernode

XINCDIRS += ../../plugins/common/include

SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS := pvmf_sm_node_factory.cpp 
SRCS +=	pvmf_streaming_manager_node.cpp
SRCS +=	pvmf_sm_fsp_registry.cpp

HDRS := pvmf_sm_node_events.h \
	    pvmf_sm_node_factory.h \
	    pvmf_sm_fsp_registry_interface.h \
	    pvmf_sm_fsp_registry_populator_interface.h \
	    pvmf_sm_fsp_shared_lib_interface.h

include $(MK)/library.mk
