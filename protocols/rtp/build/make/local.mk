# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := rtprtcp




XINCDIRS += ../../include



SRCDIR := ../../src
INCSRCDIR := ../../src

SRCS := rtcp.cpp \
	rtcp_decoder.cpp \
	rtcp_encoder.cpp

HDRS := rtprtcp.h \
	rtcp.h \
	rtcp_decoder.h \
	rtcp_encoder.h \
	rtcp_constants.h

include $(MK)/library.mk

