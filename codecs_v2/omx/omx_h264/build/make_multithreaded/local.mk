# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk


TARGET := omx_avc_component_lib

XINCDIRS += ../../../../../extern_libs_v2/khronos/openmax/include \
            ../../../../omx/omx_baseclass/include

SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS := omx_avc_component.cpp

HDRS := omx_avc_component.h


include $(MK)/library.mk

