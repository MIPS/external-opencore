#RTSP Unicast Plugin

XINCDIRS += -I../../../../../protocols/rtsp_client_engine/src
XINCDIRS += -I../../plugins/rtspunicast/include
XINCDIRS += -I../../plugins/rtsptunicast/include
XINCDIRS += -I../../../../streaming/jitterbuffernode/jitterbuffer/rtp/include

SRCS += ../plugins/rtspunicast/src/pvmf_sm_rtsp_unicast_node_factory.cpp
SRCS += ../plugins/rtspunicast/src/pvmf_sm_fsp_rtsp_unicast.cpp
SRCS += ../plugins/rtsptunicast/src/pvmf_sm_rtspt_unicast_node_factory.cpp
SRCS += ../plugins/rtsptunicast/src/pvmf_sm_fsp_rtspt_unicast.cpp
