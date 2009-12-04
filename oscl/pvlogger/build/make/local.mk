# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvlogger


SRCDIR := ../../src
INCSRCDIR := ../../src

SRCS :=	\
    pvlogger_cfg_file_parser.cpp 

HDRS := \
    pvlogger_cfg_file_parser.h \
    pvlogger_file_appender.h \
    pvlogger_mem_appender.h \
    pvlogger_stderr_appender.h \
    pvlogger_time_and_id_layout.h \
    pvlogger_empty_layout.h


include $(MK)/library.mk

