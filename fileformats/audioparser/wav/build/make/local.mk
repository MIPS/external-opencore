# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := wavparser


SRCDIR := ../../src
INCSRCDIR := ../../include

# compose final src list for actual build
SRCS :=	pvmf_wav_parser.cpp

HDRS :=	pvmf_wav_parser.h

include $(MK)/library.mk
