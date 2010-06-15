# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := audioparser


SRCDIR := ../../src
INCSRCDIR := ../../include

# compose final src list for actual build
SRCS :=	pvmf_audio_parser.cpp \
	pvmf_file_parser.cpp

HDRS :=	pvmf_audio_parser.h \
        pvmf_fileparser_interface.h \
	pvmf_fileparser_observer.h

include $(MK)/library.mk
