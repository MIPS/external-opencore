# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvmp3ffparsernode






SRCDIR := ../../src

INCSRCDIR := ../../include

SRCS := pvmf_mp3ffparser_node.cpp pvmf_mp3ffparser_outport.cpp pvmf_mp3ffparser_factory.cpp

HDRS := pvmf_mp3ffparser_factory.h pvmf_mp3ffparser_defs.h



include $(MK)/library.mk
