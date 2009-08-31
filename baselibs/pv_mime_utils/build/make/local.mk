# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvmimeutils


SRCDIR := ../../src
INCSRCDIR := ../../src

# compose final src list for actual build
SRCS :=	pv_mime_string_utils.cpp

HDRS :=  pv_mime_string_utils.h

include $(MK)/library.mk
