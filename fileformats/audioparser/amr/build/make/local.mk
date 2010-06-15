# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := amrparser


SRCDIR := ../../src
INCSRCDIR := ../../include

# compose final src list for actual build
SRCS :=	pvmf_amr_parser.cpp

HDRS :=	pvmf_amr_parser.h

include $(MK)/library.mk
