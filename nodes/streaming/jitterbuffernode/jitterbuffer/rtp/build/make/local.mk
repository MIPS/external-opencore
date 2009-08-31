# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvjitterbufferrtp


# Added this to for osclconfig.h
XINCDIRS += ../../../../../../common/include  ../../../../../streamingmanager/include ../../../../../../../protocols/rtp/src
XINCDIRS += ../../../../../common/include
XINCDIRS += ../../../common/include


SRCDIR := ../../src
INCSRCDIR := ../../include

# compose final src list for actual build
SRCS = 	pvmf_rtp_jitter_buffer_factory.cpp\
		pvmf_rtp_jitter_buffer_impl.cpp

HDRS =  pvmf_rtp_jitter_buffer_factory.h

include $(MK)/library.mk

