
#FSP Base
XINCDIRS += -I../../config/default
XINCDIRS += -I../../plugins/rtspunicast/include
XINCDIRS += -I../../plugins/rtsptunicast/include
XINCDIRS += -I../../plugins/unicastpvr/include
XINCDIRS += -I../../plugins/pvrfileplayback/include
XINCDIRS += -I../../plugins/broadcastpvr/include
XINCDIRS += -I../../plugins/common/src
XINCDIRS += -I../../plugins/mshttp/include -I../../plugins/mshttp/config
XINCDIRS += -I../../../../pvprotocolenginenode/base/src -I../../../../pvprotocolenginenode/protocol_common/src
XINCDIRS += -I../../../../../protocols/rtsp_client_engine/src
XINCDIRS += -I../../../../streaming/jitterbuffernode/jitterbuffer/rtp/include
XINCDIRS += -I../../../../streaming/jitterbuffernode/jitterbuffer/asf/include

SRCS += ../config/default/pvmf_sm_fsp_registry.cpp
SRCS += ../plugins/common/src/pvmf_sm_fsp_base_impl.cpp
SRCS += ../plugins/common/src/pvmf_sm_fsp_base_cpm_support.cpp
SRCS += ../plugins/common/src/pvmf_sm_fsp_pvr_base_impl.cpp
SRCS += ../plugins/rtspunicast/src/pvmf_sm_rtsp_unicast_node_factory.cpp
SRCS += ../plugins/rtspunicast/src/pvmf_sm_fsp_rtsp_unicast.cpp
SRCS += ../plugins/rtsptunicast/src/pvmf_sm_rtspt_unicast_node_factory.cpp
SRCS += ../plugins/rtsptunicast/src/pvmf_sm_fsp_rtspt_unicast.cpp
SRCS += ../plugins/unicastpvr/src/pvmf_sm_fsp_rtsp_unicast_plus_pvr.cpp
SRCS += ../plugins/unicastpvr/src/pvmf_sm_fsp_rtsp_unicast_pvr_support.cpp
SRCS += ../plugins/unicastpvr/src/pvmf_sm_fsp_rtsp_unicast_plus_pvr_node_factory.cpp
SRCS += ../plugins/pvrfileplayback/src/pvmf_sm_fsp_pvr_fileplayback.cpp
SRCS += ../plugins/pvrfileplayback/src/pvmf_sm_pvr_fileplayback_node_factory.cpp
SRCS += ../plugins/broadcastpvr/src/pvmf_sm_fsp_rtsp_broadcast.cpp
SRCS += ../plugins/broadcastpvr/src/pvmf_sm_fsp_rtsp_broadcast_pvr_support.cpp
SRCS += ../plugins/broadcastpvr/src/pvmf_sm_fsp_rtsp_broadcast_node_factory.cpp
SRCS += ../plugins/mshttp/src/pvmf_sm_mshttp_node_factory.cpp
SRCS += ../plugins/mshttp/src/pvmf_sm_fsp_mshttp.cpp
SRCS += ../plugins/mshttp/src/pvmf_sm_mshttp_asf_support.cpp
