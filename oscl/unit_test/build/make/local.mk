# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := unit_test


SRCDIR := ../../src
INCSRCDIR := ../../src

SRCS := test_case.cpp \
	test_problem.cpp \
	test_result.cpp \
	text_test_interpreter.cpp \
	xml_test_interpreter.cpp \
	unit_test_main.cpp \
	unit_test_args.cpp \
	unit_test_xml_writer.cpp 

HDRS := stringable.h \
	test_case.h \
	test_problem.h \
	test_result.h \
	text_test_interpreter.h \
	xml_test_interpreter.h \
	unit_test_common.h \
	unit_test_local_string.h \
	unit_test_args.h \
	unit_test_vector.h \
	unit_test_xml_writer.h

include $(MK)/library.mk



