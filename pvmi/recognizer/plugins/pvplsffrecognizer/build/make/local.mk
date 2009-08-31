# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvplsffrecognizer

XCXXFLAGS += $(FLAG_COMPILE_WARNINGS_AS_ERRORS)

SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS := pvplsffrec_factory.cpp \
        pvplsffrec_plugin.cpp

HDRS := pvplsffrec_factory.h

include $(MK)/library.mk
