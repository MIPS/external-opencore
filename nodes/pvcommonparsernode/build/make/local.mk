# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvcommonparsernode

SRCDIR := ../../src
INCSRCDIR := ../../include

# compose final src list for actual build
SRCS :=	 \
	pvmf_parsernode_impl.cpp \
	pvmf_parsernode_port.cpp \
	pvmf_parsernode_factory.cpp	

HDRS :=	\
	pvmf_parsernode_factory.h \
	pvmf_parsernode_impl.h \
	pvmf_parsernode_port.h

include $(MK)/library.mk
