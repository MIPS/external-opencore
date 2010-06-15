# This makefile is used only for building a static library 

# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := protocolenginenode

XINCDIRS +=  ../../config/$(BUILD_ARCH)  ../../config/linux
XINCDIRS +=  ../../include ../../base/src ../../protocol_common/src ../../download_protocols/common/src ../../download_protocols/progressive_download/src ../../download_protocols/progressive_streaming/src ../../download_protocols/shoutcast/src ../../download_protocols/fasttrack_download/src ../../download_protocols/rtmp_streaming/src ../../download_protocols/smooth_streaming/src ../../download_protocols/apple_http_streaming/src ../../wm_http_streaming/src ../../../../protocols/rtmp_parcom/include  ../../../../protocols/smooth_streaming/include ../../../../protocols/m3u/include

SRCDIR := ../../base/src 
INCSRCDIR := ../../include

SRCS := pvmf_protocol_engine_node.cpp \
	pvmf_protocol_engine_factory.cpp \
	pvmf_protocol_engine_port.cpp \
	pvmf_protocol_engine_node_registry.cpp \
	\
	../../protocol_common/src/pvmf_protocol_engine_common.cpp \
	../../protocol_common/src/pvmf_protocol_engine_node_common.cpp 


ifeq ($(download_common_enabled),y)
SRCS += \
	../../download_protocols/common/src/pvdl_config_file.cpp \
	../../download_protocols/common/src/pvmf_protocol_engine_download_common.cpp \
	../../download_protocols/common/src/pvmf_protocol_engine_node_download_common.cpp 
endif

ifeq ($(penode_pdl_support),y)
SRCS += \
	../../download_protocols/progressive_download/src/pvmf_protocol_engine_progressive_download.cpp \
	../../download_protocols/progressive_download/src/pvmf_protocol_engine_node_progressive_download.cpp \
	../../download_protocols/progressive_download/src/pvmf_protocol_engine_node_progressive_download_container_factory.cpp 
endif

ifeq ($(penode_ps_support),y)
SRCS += \
	../../download_protocols/progressive_streaming/src/pvmf_protocol_engine_node_progressive_streaming.cpp \
	../../download_protocols/progressive_streaming/src/pvmf_protocol_engine_node_progressive_streaming_container_factory.cpp 
endif

ifeq ($(penode_shoutcast_support),y)
SRCS += \
	../../download_protocols/shoutcast/src/pvmf_protocol_engine_shoutcast.cpp \
	../../download_protocols/shoutcast/src/pvmf_protocol_engine_node_shoutcast.cpp \
	../../download_protocols/shoutcast/src/pvmf_protocol_engine_node_shoutcast_container_factory.cpp 
endif

ifeq ($(penode_rtmp_support),y)
SRCS += \
	../../download_protocols/rtmp_streaming/src/pvmf_protocol_engine_rtmp_streaming.cpp \
	../../download_protocols/rtmp_streaming/src/pvmf_protocol_engine_node_rtmp_streaming.cpp \
	../../download_protocols/rtmp_streaming/src/pvmf_protocol_engine_node_rtmp_streaming_container_factory.cpp 
endif

ifeq ($(ss_or_als),y)
SRCS += \
	../../download_protocols/common/src/pvmf_protocol_engine_tcpsocket.cpp 
endif

ifeq ($(penode_smooth_streaming_support),y)
SRCS += \
	../../download_protocols/smooth_streaming/src/pvmf_protocol_engine_smooth_streaming.cpp \
	../../download_protocols/smooth_streaming/src/pvmf_protocol_engine_node_smooth_streaming.cpp \
	../../download_protocols/smooth_streaming/src/pvmf_protocol_engine_node_smooth_streaming_container_factory.cpp \
	../../download_protocols/smooth_streaming/src/pvmf_protocol_engine_node_smooth_streaming_ppb.cpp \
	../../download_protocols/smooth_streaming/src/pvmf_protocol_engine_node_smooth_streaming_ppb_container_factory.cpp
endif

ifeq ($(penode_apple_http_streaming_support),y)
SRCS += \
	../../download_protocols/apple_http_streaming/src/pvmf_protocol_engine_apple_http_streaming.cpp \
	../../download_protocols/apple_http_streaming/src/pvmf_protocol_engine_node_apple_http_streaming.cpp \
	../../download_protocols/apple_http_streaming/src/pvmf_protocol_engine_node_apple_http_streaming_container_factory.cpp
endif

ifeq ($(penode_ftdl_support),y)
SRCS += \
	../../download_protocols/fasttrack_download/src/pvmf_protocol_engine_fasttrack_download.cpp \
	../../download_protocols/fasttrack_download/src/pvmf_protocol_engine_node_fasttrack_download.cpp \
	../../download_protocols/fasttrack_download/src/pvmf_protocol_engine_node_fasttrack_download_container_factory.cpp 
endif

ifeq ($(penode_wmhttpstreaming_support),y)
SRCS += \
	../../wm_http_streaming/src/pvmf_protocol_engine_wmhttpstreaming.cpp \
	../../wm_http_streaming/src/pvmf_protocol_engine_node_wmhttpstreaming.cpp \
	../../wm_http_streaming/src/pvms_http_streaming_parser.cpp \
	../../wm_http_streaming/src/xml_composer.cpp \
	../../wm_http_streaming/src/pvmf_protocol_engine_node_wm_http_streaming_container_factory.cpp 
endif	

HDRS := pvmf_protocol_engine_factory.h \
	pvmf_protocol_engine_defs.h \
	pvmf_protocol_engine_node_extension.h \
	pvmf_protocol_engine_command_format_ids.h \
	pvmf_protocol_engine_node_events.h

include $(MK)/library.mk

