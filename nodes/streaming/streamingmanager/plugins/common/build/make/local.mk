# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvsmfspcommon

SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS =	pvmf_sm_fsp_base_cpm_support.cpp
SRCS +=	pvmf_sm_fsp_base_impl.cpp

include $(MK)/library.mk
