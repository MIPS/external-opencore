# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvrgb12toyuv420

OPTIMIZE_FOR_PERFORMANCE_OVER_SIZE := true

SRCDIR := ../../src


SRCS := ccrgb12toyuv420.cpp \
        cczoomrotationbase.cpp

include $(MK)/library.mk
