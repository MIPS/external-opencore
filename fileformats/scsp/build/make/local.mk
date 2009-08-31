# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := scsp


SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS := pvmf_shoutcast_stream_parser.cpp \
        pvplsfileparser.cpp

HDRS := pvmf_shoutcast_stream_parser.h \
	pvplsfileparser.h

include $(MK)/library.mk
