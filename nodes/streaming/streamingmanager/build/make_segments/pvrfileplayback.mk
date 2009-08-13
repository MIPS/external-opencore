
#PVR file playback Plugin

XINCDIRS += -I../../plugins/pvrfileplayback/include
XINCDIRS += -I../../plugins/common/src
XINCDIRS += -I../../../../streaming/jitterbuffernode/jitterbuffer/rtp/include

SRCS += ../plugins/pvrfileplayback/src/pvmf_sm_fsp_pvr_fileplayback.cpp
SRCS += ../plugins/pvrfileplayback/src/pvmf_sm_pvr_fileplayback_node_factory.cpp
SRCS += ../plugins/common/src/pvmf_sm_fsp_pvr_base_impl.cpp

