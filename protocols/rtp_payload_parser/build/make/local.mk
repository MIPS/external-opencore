# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := rtppayloadparser

XINCDIRS +=  ../../rfc_3016/include  ../../rfc_3267/include  ../../rfc_3016/include  ../../rfc_2429/include  ../../rfc_3984/include  ../../asf/include  ../../rfc_3984/src  ../../realmedia/include  ../../rfc_3640/include
XINCDIRS += ../../../../pvmi/pvmf/include ../../../../protocols/sdp/common/include ../../../../baselibs/gen_data_structures/src

SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS := rtp_payload_parser_base.cpp


HDRS := bit_util.h \
	payload_parser.h \
	payload_parser_factory.h \
	payload_parser_registry.h

include $(MK)/library.mk

