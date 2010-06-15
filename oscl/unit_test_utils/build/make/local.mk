# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := unit_test_utils

SRCDIR := ../../src
INCSRCDIR := ../../src

SRCS := \
    ut.cpp 

HDRS := \
    ut.h

include $(MK)/library.mk
