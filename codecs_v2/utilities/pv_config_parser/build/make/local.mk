# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pv_config_parser


XCXXFLAGS += $(FLAG_COMPILE_WARNINGS_AS_ERRORS)

OPTIMIZE_FOR_PERFORMANCE_OVER_SIZE := true

# WMA Audio build defines
ifeq ($(WMA_AUDIO_SUPPORT_ENABLED),1)
XCPPFLAGS += -DWMA_AUDIO_SUPPORTED
endif

# WMA Voice build defines
ifeq ($(WMA_VOICE_SUPPORT_ENABLED),1)
XCPPFLAGS += -DWMA_VOICE_SUPPORTED
endif

# Macro to enable old wma audio lib
ifeq ($(USE_OLD_WMA_DECODER),1)
XCPPFLAGS += -DBUILD_OLDWMAAUDIOLIB
endif

XINCDIRS = ../../../../../pvmi/pvmf/include

SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS := pv_video_config_parser.cpp \
	pv_audio_config_parser.cpp

HDRS := pv_video_config_parser.h \
	pv_audio_config_parser.h

include $(MK)/library.mk
