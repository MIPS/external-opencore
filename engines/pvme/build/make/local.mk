# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvmetadata_engine


XINCDIRS += ../../../player/include ../../../player/src ../../../player/config/core

SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS := pv_metadata_engine.cpp \
	pv_metadata_engine_factory.cpp \
	pvme_node_registry.cpp \
        ../config/default/pvme_node_registry_populator.cpp 

HDRS := pv_metadata_engine.h \
	pv_metadata_engine_factory.h \
        pv_metadata_engine_interface.h \
	pv_metadata_engine_types.h \
	pvme_node_registry.h

include $(MK)/library.mk

